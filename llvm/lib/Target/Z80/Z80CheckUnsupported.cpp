//===-- Z80CheckUnsupported.cpp - Z80 unsupported construct check ---------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80CheckUnsupported.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"

#include "Z80.h"

#define DEBUG_TYPE "z80-check-unsupported"

using namespace llvm;

// The rounding masks the low byte of the address, so that is as far as an
// alignment can reach.
static constexpr uint64_t MaxStackAlign = 128;

static cl::opt<unsigned> WarnStackAlignPadding(
    "z80-warn-stack-align-padding",
    cl::desc("Warn about an over-aligned stack object once its padding "
             "reaches this many bytes (0 disables the warning)"),
    cl::init(1), cl::Hidden);

namespace {

/// Report IR constructs this backend does not support as proper errors.
/// asm goto has no GlobalISel lowering (the IR translator refuses inline-asm
/// callbr) and would otherwise surface as an internal backend error; the
/// callbr is replaced with its fallthrough edge so compilation reaches the
/// diagnostic cleanly.
// True when one of the constraint's alternative codes lets the operand live
// in a register.
static bool hasRegisterAlternative(const InlineAsm::ConstraintInfo &C) {
  for (const std::string &Code : C.Codes)
    if (Code == "r" || Code == "R" || Code == "X" || Code[0] == '{' ||
        (Code.size() == 1 && StringRef("abcdehl").contains(Code[0])))
      return true;
  return false;
}

static bool hasMemoryAlternative(const InlineAsm::ConstraintInfo &C) {
  for (const std::string &Code : C.Codes)
    if (Code == "m" || Code == "o" || Code == "V")
      return true;
  return false;
}

static bool isImmediateOnly(const InlineAsm::ConstraintInfo &C) {
  for (const std::string &Code : C.Codes)
    if (Code.size() != 1 || !StringRef("insEF").contains(Code[0]))
      return false;
  return !C.Codes.empty();
}

// GlobalISel's inline asm lowering does not implement register outputs that
// are stored through a pointer, which is what clang emits for "+g"- and
// "=X"-style constraints. When such an output may live in a register,
// rewrite it into a plain register output followed by an explicit store, so
// the asm call only carries operand shapes the lowering implements.
static bool rewriteIndirectAsmOutputs(Function &F) {
  SmallVector<CallInst *, 4> Worklist;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *CI = dyn_cast<CallInst>(&I))
        if (CI->isInlineAsm())
          Worklist.push_back(CI);

  bool Changed = false;
  for (CallInst *CI : Worklist) {
    auto *IA = cast<InlineAsm>(CI->getCalledOperand());
    InlineAsm::ConstraintInfoVector CV = IA->ParseConstraints();

    SmallVector<bool, 8> Rewrite(CV.size(), false);
    bool Any = false;
    unsigned ArgIdx = 0;
    for (unsigned I = 0; I != CV.size(); ++I) {
      const InlineAsm::ConstraintInfo &C = CV[I];
      bool ConsumesArg = C.Type == InlineAsm::isInput ||
                         (C.Type == InlineAsm::isOutput && C.isIndirect);
      // An aggregate cannot become a direct asm result; leave it indirect
      // (a register-only aggregate is then diagnosed as unsupported).
      if (C.Type == InlineAsm::isOutput && C.isIndirect &&
          hasRegisterAlternative(C) &&
          !CI->getParamElementType(ArgIdx)->isAggregateType()) {
        Rewrite[I] = true;
        Any = true;
      }
      if (ConsumesArg)
        ++ArgIdx;
    }
    if (!Any)
      continue;

    // Constraint string segments map 1:1 to the parsed constraints.
    SmallVector<StringRef, 8> Segments;
    StringRef ConstraintStr = IA->getConstraintString();
    ConstraintStr.split(Segments, ',');
    if (Segments.size() != CV.size())
      continue;

    Type *OldRet = CI->getType();
    auto OldRetElt = [&](unsigned Idx) -> Type * {
      if (auto *ST = dyn_cast<StructType>(OldRet))
        return ST->getElementType(Idx);
      return OldRet;
    };

    unsigned OldRetIdx = 0, ArgNo = 0;
    SmallVector<Type *, 4> NewRetTypes;
    SmallVector<Value *, 8> NewArgs;
    SmallVector<AttributeSet, 8> NewArgAttrs;
    std::string NewConstraints;
    // Pointer to store through and the result index that feeds it.
    SmallVector<std::pair<Value *, unsigned>, 4> Stores;
    // New result index of each old direct output, in output order.
    SmallVector<unsigned, 4> OldToNewRet;

    for (unsigned I = 0; I != CV.size(); ++I) {
      const InlineAsm::ConstraintInfo &C = CV[I];
      if (!NewConstraints.empty())
        NewConstraints += ',';
      if (Rewrite[I]) {
        Stores.push_back({CI->getArgOperand(ArgNo), NewRetTypes.size()});
        NewRetTypes.push_back(CI->getParamElementType(ArgNo));
        ++ArgNo;
        NewConstraints += C.isEarlyClobber ? "=&r" : "=r";
        continue;
      }
      NewConstraints += Segments[I];
      if (C.Type == InlineAsm::isOutput && !C.isIndirect) {
        OldToNewRet.push_back(NewRetTypes.size());
        NewRetTypes.push_back(OldRetElt(OldRetIdx++));
      }
      if (C.Type == InlineAsm::isInput ||
          (C.Type == InlineAsm::isOutput && C.isIndirect)) {
        NewArgs.push_back(CI->getArgOperand(ArgNo));
        NewArgAttrs.push_back(CI->getAttributes().getParamAttrs(ArgNo));
        ++ArgNo;
      }
    }

    LLVMContext &Ctx = F.getContext();
    Type *NewRet = NewRetTypes.empty() ? Type::getVoidTy(Ctx)
                   : NewRetTypes.size() == 1
                       ? NewRetTypes[0]
                       : StructType::get(Ctx, NewRetTypes);
    SmallVector<Type *, 8> ParamTys;
    for (Value *V : NewArgs)
      ParamTys.push_back(V->getType());
    FunctionType *NewFTy = FunctionType::get(NewRet, ParamTys, false);
    InlineAsm *NewIA = InlineAsm::get(
        NewFTy, IA->getAsmString(), NewConstraints, IA->hasSideEffects(),
        IA->isAlignStack(), IA->getDialect(), IA->canThrow());
    CallInst *NewCall =
        CallInst::Create(NewFTy, NewIA, NewArgs, "", CI->getIterator());
    NewCall->copyMetadata(*CI);
    NewCall->setAttributes(AttributeList::get(
        Ctx, CI->getAttributes().getFnAttrs(), AttributeSet(), NewArgAttrs));

    auto ExtractRet = [&](unsigned Idx) -> Value * {
      if (NewRetTypes.size() == 1)
        return NewCall;
      return ExtractValueInst::Create(NewCall, {Idx}, "", CI->getIterator());
    };

    for (const auto &[Ptr, Idx] : Stores)
      new StoreInst(ExtractRet(Idx), Ptr, CI->getIterator());

    if (!OldRet->isVoidTy()) {
      Value *Repl;
      if (auto *ST = dyn_cast<StructType>(OldRet)) {
        Repl = PoisonValue::get(ST);
        for (unsigned I = 0; I != OldToNewRet.size(); ++I)
          Repl = InsertValueInst::Create(Repl, ExtractRet(OldToNewRet[I]), {I},
                                         "", CI->getIterator());
      } else {
        Repl = ExtractRet(OldToNewRet[0]);
      }
      CI->replaceAllUsesWith(Repl);
    }
    CI->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

// A value wider than a 16-bit register pair cannot be placed in registers,
// and the lowering also has no way to split a wide direct output. Wide
// operands are only viable through memory.
static bool hasWideDirectOperand(const CallBase &CB, const DataLayout &DL) {
  const auto *IA = cast<InlineAsm>(CB.getCalledOperand());
  Type *Ret = CB.getType();
  unsigned RetIdx = 0, ArgNo = 0;
  for (const InlineAsm::ConstraintInfo &C : IA->ParseConstraints()) {
    if (C.Type == InlineAsm::isClobber || C.Type == InlineAsm::isLabel)
      continue;
    if (C.Type == InlineAsm::isOutput) {
      if (C.isIndirect) {
        // Register-only indirect outputs survive the rewrite only when the
        // pointee is an aggregate, which no register sequence can carry.
        if (hasRegisterAlternative(C) && !hasMemoryAlternative(C))
          return true;
        ++ArgNo;
        continue;
      }
      Type *Ty = isa<StructType>(Ret)
                     ? cast<StructType>(Ret)->getElementType(RetIdx)
                     : Ret;
      ++RetIdx;
      if (DL.getTypeSizeInBits(Ty) > 16)
        return true;
      continue;
    }
    Value *Op = CB.getArgOperand(ArgNo++);
    if (C.isIndirect) {
      // Same for indirect inputs: only memory can carry them.
      if (hasRegisterAlternative(C) && !hasMemoryAlternative(C))
        return true;
      continue;
    }
    if (DL.getTypeSizeInBits(Op->getType()) <= 16)
      continue;
    // A tied input mirrors its output, which was already checked.
    if (!C.Codes.empty() && isDigit(C.Codes[0][0]))
      continue;
    // The lowering spills these to a stack slot itself.
    if (hasMemoryAlternative(C))
      continue;
    if (isImmediateOnly(C) && isa<Constant>(Op))
      continue;
    return true;
  }
  return false;
}

// SP is at an arbitrary address when a function is entered and there is no
// callee-saved register to hold it across a realignment, so the stack itself
// stays byte-aligned. An object that wants more gets its alignment minus one
// in extra bytes, and its address is rounded up inside that room.
static bool lowerOverAlignedAllocas(Function &F, const DataLayout &DL) {
  SmallVector<AllocaInst *, 4> Worklist;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *AI = dyn_cast<AllocaInst>(&I))
        if (AI->getAlign() > Align(1))
          Worklist.push_back(AI);

  bool Changed = false;
  for (AllocaInst *AI : Worklist) {
    const uint64_t A = AI->getAlign().value();
    if (A > MaxStackAlign) {
      F.getContext().diagnose(DiagnosticInfoUnsupported(
          F,
          "cannot align a stack object to " + Twine(A) + ", the maximum is " +
              Twine(MaxStackAlign) + "; use a static object for aligned data",
          AI->getDebugLoc()));
      // Drop the request so the frame layout check does not repeat the error.
      AI->setAlignment(Align(1));
      Changed = true;
      continue;
    }

    // A dynamically sized object would have to size its own padding; SM83
    // refuses those in the legalizer anyway.
    if (!AI->isStaticAlloca())
      continue;

    const uint64_t Extra = A - 1;
    const uint64_t Size =
        DL.getTypeAllocSize(AI->getAllocatedType()).getFixedValue() *
        cast<ConstantInt>(AI->getArraySize())->getZExtValue();

    if (WarnStackAlignPadding && Extra >= WarnStackAlignPadding)
      F.getContext().diagnose(DiagnosticInfoUnsupported(
          F,
          "aligning a stack object to " + Twine(A) + " costs " + Twine(Extra) +
              (Extra == 1 ? " byte" : " bytes") +
              " of padding; use a static object for aligned data",
          AI->getDebugLoc(), DS_Warning));

    IRBuilder<> B(AI);
    Type *I8 = Type::getInt8Ty(F.getContext());
    auto *Raw = B.CreateAlloca(ArrayType::get(I8, Size + Extra), nullptr,
                               AI->getName() + ".raw");
    Raw->setAlignment(Align(1));

    Type *IntPtrTy = DL.getIntPtrType(AI->getType());
    const unsigned Bits = IntPtrTy->getIntegerBitWidth();
    Value *Addr = B.CreatePtrToInt(Raw, IntPtrTy);
    Addr = B.CreateAdd(Addr, ConstantInt::get(IntPtrTy, Extra));
    Addr = B.CreateAnd(Addr, ConstantInt::get(IntPtrTy, ~APInt(Bits, Extra)));
    Value *Aligned = B.CreateIntToPtr(Addr, AI->getType(), AI->getName());

    // llvm.lifetime.* takes an alloca and nothing else, and the range it
    // marks is the whole allocation, so those uses move to the raw object
    // rather than to the rounded address inside it.
    SmallVector<IntrinsicInst *, 2> Lifetimes;
    for (User *U : AI->users())
      if (auto *II = dyn_cast<IntrinsicInst>(U))
        if (II->getIntrinsicID() == Intrinsic::lifetime_start ||
            II->getIntrinsicID() == Intrinsic::lifetime_end)
          Lifetimes.push_back(II);

    AI->replaceAllUsesWith(Aligned);
    for (IntrinsicInst *II : Lifetimes)
      II->replaceUsesOfWith(Aligned, Raw);
    AI->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

class Z80CheckUnsupported : public FunctionPass {
public:
  static char ID;
  Z80CheckUnsupported() : FunctionPass(ID) {
    llvm::initializeZ80CheckUnsupportedPass(*PassRegistry::getPassRegistry());
  }
  StringRef getPassName() const override {
    return "Z80 unsupported construct check";
  }
  bool runOnFunction(Function &F) override {
    // Atomic read-modify-write has no honest implementation here: nothing
    // stops an interrupt handler between the load and the store, and the
    // interrupt state cannot be reliably saved and restored to close that
    // window (IFF1 is unreadable without the NMI erratum, SM83's IME is
    // unreadable entirely). Refuse rather than pretend.
    SmallVector<Instruction *, 2> Atomics;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (isa<AtomicRMWInst>(I) || isa<AtomicCmpXchgInst>(I))
          Atomics.push_back(&I);
    for (Instruction *I : Atomics) {
      F.getContext().diagnose(DiagnosticInfoUnsupported(
          F, "atomic read-modify-write operations are not supported",
          I->getDebugLoc()));
      if (!I->getType()->isVoidTy())
        I->replaceAllUsesWith(PoisonValue::get(I->getType()));
      I->eraseFromParent();
    }

    SmallVector<CallBrInst *, 2> AsmGotos;
    for (BasicBlock &BB : F)
      if (auto *CBR = dyn_cast<CallBrInst>(BB.getTerminator()))
        if (CBR->isInlineAsm())
          AsmGotos.push_back(CBR);

    for (CallBrInst *CBR : AsmGotos) {
      F.getContext().diagnose(DiagnosticInfoUnsupported(
          F, "asm goto is not supported", CBR->getDebugLoc()));

      BasicBlock *Parent = CBR->getParent();
      BasicBlock *DefaultDest = CBR->getDefaultDest();
      // Every entry in the indirect list is its own edge with its own PHI
      // entry, even when it repeats a block or the default destination.
      // The replacing branch keeps exactly one edge (the default), so drop
      // one PHI entry per indirect entry.
      for (BasicBlock *Ind : CBR->getIndirectDests())
        Ind->removePredecessor(Parent);
      if (!CBR->getType()->isVoidTy())
        CBR->replaceAllUsesWith(PoisonValue::get(CBR->getType()));
      UncondBrInst::Create(DefaultDest, CBR->getIterator());
      CBR->eraseFromParent();
    }

    bool Changed = !Atomics.empty() || !AsmGotos.empty();
    Changed |= rewriteIndirectAsmOutputs(F);

    const DataLayout &DL = F.getParent()->getDataLayout();
    Changed |= lowerOverAlignedAllocas(F, DL);

    SmallVector<CallInst *, 2> WideAsm;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *CI = dyn_cast<CallInst>(&I))
          if (CI->isInlineAsm() && hasWideDirectOperand(*CI, DL))
            WideAsm.push_back(CI);
    for (CallInst *CI : WideAsm) {
      F.getContext().diagnose(DiagnosticInfoUnsupported(
          F, "unsupported inline asm operand: value wider than 16 bits",
          CI->getDebugLoc()));
      if (!CI->getType()->isVoidTy())
        CI->replaceAllUsesWith(PoisonValue::get(CI->getType()));
      CI->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }
};

} // namespace

char Z80CheckUnsupported::ID = 0;

INITIALIZE_PASS(Z80CheckUnsupported, DEBUG_TYPE,
                "Z80 unsupported construct check", false, false)

FunctionPass *llvm::createZ80CheckUnsupportedPass() {
  return new Z80CheckUnsupported();
}
