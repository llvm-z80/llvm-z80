//===-- Z80NarrowMemAccess.cpp - Narrow wide integer memory access --------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The middle end handles a small aggregate as one wide integer: InstCombine
// turns an eight-byte memcpy into an i64 load and store, and SROA then writes
// a field update on such a copy as a shift and mask on the i64. Updating one
// byte of a struct becomes a load of all eight bytes, an and, an or and a
// store of all eight bytes back. The legalizer splits each of those into
// 16-bit pieces, and the pieces outnumber the register pairs.
//
// This pass takes the update back apart at the byte level. A store whose
// bytes, followed through bitwise operations, byte-multiple shifts and
// extensions, are the bytes a load just read from the same place keeps only
// the bytes that change. A load whose uses each take out a few of its bytes
// becomes loads of just those bytes.
//
//===----------------------------------------------------------------------===//

#include "Z80NarrowMemAccess.h"

#include "Z80.h"
#include "Z80InstrInfo.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"

#define DEBUG_TYPE "z80-narrow-mem-access"

using namespace llvm;

STATISTIC(NumStoresNarrowed,
          "Number of wide stores cut down to the bytes they change");
STATISTIC(NumStoresRemoved,
          "Number of wide stores that wrote back the bytes just loaded");
STATISTIC(NumLoadsNarrowed, "Number of wide loads cut down to the bytes used");

namespace {

// Byte Idx of V. A null V stands for a byte known to be zero.
struct ByteSrc {
  Value *V = nullptr;
  unsigned Idx = 0;
};

// The width of Ty in bytes, or zero when it is not an integer of whole bytes.
unsigned numBytes(Type *Ty) {
  auto *IT = dyn_cast<IntegerType>(Ty);
  if (!IT || IT->getBitWidth() % 8)
    return 0;
  return IT->getBitWidth() / 8;
}

std::optional<uint8_t> constByte(const ByteSrc &S) {
  if (auto *C = dyn_cast_or_null<ConstantInt>(S.V))
    return C->getValue().extractBitsAsZExtValue(8, 8 * S.Idx);
  return std::nullopt;
}

// Follows byte K of V back through operations that move whole bytes around
// without mixing them. What comes back is always a true description of the
// byte; the walk just stops early when it runs out of budget. SROA writes
// each field of a copy with an and and an or, so an eight-field update is a
// few dozen steps.
ByteSrc resolveByte(Value *V, unsigned K, unsigned &Budget) {
  if (auto *C = dyn_cast<ConstantInt>(V))
    return C->getValue().extractBitsAsZExtValue(8, 8 * K) ? ByteSrc{V, K}
                                                          : ByteSrc{};
  auto *I = dyn_cast<Instruction>(V);
  if (!I || !Budget)
    return {V, K};
  --Budget;

  unsigned Bytes = numBytes(I->getType());
  switch (I->getOpcode()) {
  case Instruction::ZExt: {
    unsigned SrcBytes = numBytes(I->getOperand(0)->getType());
    if (!SrcBytes)
      break;
    if (K >= SrcBytes)
      return {};
    return resolveByte(I->getOperand(0), K, Budget);
  }
  case Instruction::Trunc:
    if (!numBytes(I->getOperand(0)->getType()))
      break;
    return resolveByte(I->getOperand(0), K, Budget);
  case Instruction::Shl:
  case Instruction::LShr: {
    auto *Amt = dyn_cast<ConstantInt>(I->getOperand(1));
    if (!Amt || Amt->getValue().uge(8 * Bytes) || Amt->getZExtValue() % 8)
      break;
    unsigned Shift = Amt->getZExtValue() / 8;
    if (I->getOpcode() == Instruction::Shl) {
      if (K < Shift)
        return {};
      return resolveByte(I->getOperand(0), K - Shift, Budget);
    }
    if (K + Shift >= Bytes)
      return {};
    return resolveByte(I->getOperand(0), K + Shift, Budget);
  }
  case Instruction::Or:
  case Instruction::Xor: {
    ByteSrc A = resolveByte(I->getOperand(0), K, Budget);
    ByteSrc B = resolveByte(I->getOperand(1), K, Budget);
    if (!A.V)
      return B;
    if (!B.V)
      return A;
    break;
  }
  case Instruction::And: {
    ByteSrc A = resolveByte(I->getOperand(0), K, Budget);
    ByteSrc B = resolveByte(I->getOperand(1), K, Budget);
    if (!A.V || !B.V)
      return {};
    if (constByte(B) == 0xFF)
      return A;
    if (constByte(A) == 0xFF)
      return B;
    break;
  }
  default:
    break;
  }
  return {V, K};
}

ByteSrc resolveByte(Value *V, unsigned K) {
  unsigned Budget = 256;
  return resolveByte(V, K, Budget);
}

Value *extractByte(IRBuilder<> &B, Value *V, unsigned K) {
  if (K)
    V = B.CreateLShr(V, 8 * K);
  return B.CreateTrunc(V, B.getInt8Ty());
}

// Builds the i8 values of bytes found by resolveByte.
class ByteBuilder {
  IRBuilder<> &B;
  DenseMap<std::pair<Value *, unsigned>, Value *> Built;

public:
  explicit ByteBuilder(IRBuilder<> &B) : B(B) {}

  Value *build(ByteSrc S) {
    if (!S.V)
      return B.getInt8(0);
    if (std::optional<uint8_t> C = constByte(S))
      return B.getInt8(*C);
    auto Key = std::make_pair(S.V, S.Idx);
    if (Value *V = Built.lookup(Key))
      return V;

    Value *R;
    auto *BO = dyn_cast<BinaryOperator>(S.V);
    if (BO && (BO->getOpcode() == Instruction::Or ||
               BO->getOpcode() == Instruction::Xor ||
               BO->getOpcode() == Instruction::And)) {
      // The bitwise operations work on each byte on its own.
      Value *A = build(resolveByte(BO->getOperand(0), S.Idx));
      Value *C = build(resolveByte(BO->getOperand(1), S.Idx));
      R = B.CreateBinOp(BO->getOpcode(), A, C);
    } else {
      R = extractByte(B, S.V, S.Idx);
    }
    Built[Key] = R;
    return R;
  }
};

Value *byteAddress(IRBuilder<> &B, const DataLayout &DL, Value *Ptr,
                   unsigned Off) {
  if (!Off)
    return Ptr;
  // The wide access covered this byte, so it lies inside the same object.
  return B.CreateInBoundsPtrAdd(
      Ptr, ConstantInt::get(DL.getIndexType(Ptr->getType()), Off));
}

// Nothing between From and To can write memory. Both are in one block, From
// first.
bool noWriteBetween(Instruction *From, Instruction *To) {
  for (Instruction *I = From->getNextNode(); I != To; I = I->getNextNode())
    if (I->mayWriteToMemory())
      return false;
  return true;
}

bool isNarrowable(const LoadInst *L) {
  return L->isSimple() && L->getPointerAddressSpace() == Z80::AS_Memory;
}

// Cuts a store down to the bytes that differ from what a load in the same
// block read from the same address, with nothing written in between.
bool narrowStore(StoreInst *S, const DataLayout &DL) {
  if (!S->isSimple() || S->getPointerAddressSpace() != Z80::AS_Memory)
    return false;
  Value *V = S->getValueOperand();
  unsigned Bytes = numBytes(V->getType());
  if (Bytes < 2)
    return false;

  unsigned IdxWidth = DL.getIndexTypeSizeInBits(S->getPointerOperandType());
  APInt StoreOff(IdxWidth, 0);
  const Value *StoreBase =
      S->getPointerOperand()->stripAndAccumulateConstantOffsets(
          DL, StoreOff, /*AllowNonInbounds=*/true);

  // Whether byte K of the stored value is what memory already holds there.
  DenseMap<LoadInst *, bool> Unclobbered;
  auto IsUnchanged = [&](const ByteSrc &Src, unsigned K) {
    auto *L = dyn_cast_or_null<LoadInst>(Src.V);
    if (!L || !isNarrowable(L) || L->getParent() != S->getParent() ||
        !L->comesBefore(S))
      return false;
    APInt LoadOff(IdxWidth, 0);
    const Value *LoadBase =
        L->getPointerOperand()->stripAndAccumulateConstantOffsets(
            DL, LoadOff, /*AllowNonInbounds=*/true);
    if (LoadBase != StoreBase || LoadOff + Src.Idx != StoreOff + K)
      return false;
    auto [It, Inserted] = Unclobbered.try_emplace(L);
    if (Inserted)
      It->second = noWriteBetween(L, S);
    return It->second;
  };

  SmallVector<ByteSrc, 8> Src(Bytes);
  SmallVector<unsigned, 8> Changed;
  for (unsigned K = 0; K != Bytes; ++K) {
    Src[K] = resolveByte(V, K);
    if (!IsUnchanged(Src[K], K))
      Changed.push_back(K);
  }
  // Rebuilding the changed bytes one by one only pays when it replaces the
  // wide computation rather than adding to it.
  if (Changed.size() == Bytes || (!Changed.empty() && !V->hasOneUse()))
    return false;

  LLVM_DEBUG(dbgs() << "Z80NarrowMemAccess: " << Changed.size() << " of "
                    << Bytes << " bytes change in " << *S << "\n");
  IRBuilder<> B(S);
  ByteBuilder Builder(B);
  for (unsigned K : Changed) {
    StoreInst *NS = B.CreateAlignedStore(
        Builder.build(Src[K]), byteAddress(B, DL, S->getPointerOperand(), K),
        commonAlignment(S->getAlign(), K));
    NS->setAAMetadata(S->getAAMetadata().adjustForAccess(K, B.getInt8Ty(), DL));
  }
  if (Changed.empty())
    ++NumStoresRemoved;
  else
    ++NumStoresNarrowed;
  S->eraseFromParent();
  RecursivelyDeleteTriviallyDeadInstructions(V);
  return true;
}

// Replaces a load whose uses only take out some of its bytes with loads of
// those bytes, as long as they are at most half of it. A 16-bit load stays:
// Z80 reads a global's two bytes into a pair with one instruction, and one
// byte only into A.
bool narrowLoad(LoadInst *L, const DataLayout &DL,
                SmallVectorImpl<LoadInst *> &Worklist) {
  unsigned Bytes = numBytes(L->getType());
  if (Bytes < 3 || Bytes > 32 || !isNarrowable(L))
    return false;

  // A use that reads Width bytes at Off, possibly zero-extended to its type.
  struct Extract {
    Instruction *I;
    unsigned Off;
    unsigned Width;
  };
  SmallVector<Extract, 4> Extracts;
  SmallVector<Instruction *, 4> Shifts;
  uint32_t Covered = 0;
  auto AddExtract = [&](Instruction *I, unsigned Off) {
    unsigned Width = std::min(numBytes(I->getType()), Bytes - Off);
    if (Width != 1 && Width != 2 && Width != 4)
      return false;
    Extracts.push_back({I, Off, Width});
    Covered |= ((uint32_t(1) << Width) - 1) << Off;
    return true;
  };

  for (User *U : L->users()) {
    auto *I = cast<Instruction>(U);
    if (isa<TruncInst>(I)) {
      if (!AddExtract(I, 0))
        return false;
      continue;
    }
    if (I->getOpcode() != Instruction::LShr || I->getOperand(0) != L)
      return false;
    auto *Amt = dyn_cast<ConstantInt>(I->getOperand(1));
    if (!Amt || Amt->getValue().uge(8 * Bytes) || Amt->getZExtValue() % 8)
      return false;
    unsigned Off = Amt->getZExtValue() / 8;
    if (all_of(I->users(), [](User *T) { return isa<TruncInst>(T); })) {
      for (User *T : I->users())
        if (!AddExtract(cast<Instruction>(T), Off))
          return false;
      Shifts.push_back(I);
    } else if (!AddExtract(I, Off)) {
      return false;
    }
  }
  unsigned Used = llvm::popcount(Covered);
  if (Extracts.empty() || 2 * Used > Bytes)
    return false;

  LLVM_DEBUG(dbgs() << "Z80NarrowMemAccess: " << Used << " of " << Bytes
                    << " bytes used from " << *L << "\n");
  // Read at the wide load's place, where memory holds what it read.
  IRBuilder<> B(L);
  DenseMap<std::pair<unsigned, unsigned>, LoadInst *> Made;
  for (const Extract &E : Extracts) {
    LoadInst *&NL = Made[{E.Off, E.Width}];
    if (!NL) {
      Type *Ty = B.getIntNTy(8 * E.Width);
      NL = B.CreateAlignedLoad(
          Ty, byteAddress(B, DL, L->getPointerOperand(), E.Off),
          commonAlignment(L->getAlign(), E.Off));
      NL->setAAMetadata(L->getAAMetadata().adjustForAccess(E.Off, Ty, DL));
      if (E.Width > 2)
        Worklist.push_back(NL);
    }
    E.I->replaceAllUsesWith(B.CreateZExt(NL, E.I->getType()));
  }
  for (const Extract &E : Extracts)
    E.I->eraseFromParent();
  for (Instruction *I : Shifts)
    I->eraseFromParent();
  L->eraseFromParent();
  ++NumLoadsNarrowed;
  return true;
}

class Z80NarrowMemAccess : public FunctionPass {
public:
  static char ID;

  Z80NarrowMemAccess() : FunctionPass(ID) {
    llvm::initializeZ80NarrowMemAccessPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Z80 narrow wide memory access";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;
    const DataLayout &DL = F.getDataLayout();

    // Stores first: they turn their loads' remaining uses into byte
    // extracts, which the loads then take over.
    SmallVector<StoreInst *, 16> Stores;
    for (Instruction &I : instructions(F))
      if (auto *S = dyn_cast<StoreInst>(&I))
        Stores.push_back(S);
    bool Changed = false;
    for (StoreInst *S : Stores)
      Changed |= narrowStore(S, DL);

    SmallVector<LoadInst *, 16> Loads;
    for (Instruction &I : instructions(F))
      if (auto *L = dyn_cast<LoadInst>(&I))
        Loads.push_back(L);
    while (!Loads.empty())
      Changed |= narrowLoad(Loads.pop_back_val(), DL, Loads);
    return Changed;
  }
};

} // namespace

char Z80NarrowMemAccess::ID = 0;

INITIALIZE_PASS(Z80NarrowMemAccess, DEBUG_TYPE, "Z80 narrow wide memory access",
                false, false)

FunctionPass *llvm::createZ80NarrowMemAccessPass() {
  return new Z80NarrowMemAccess();
}
