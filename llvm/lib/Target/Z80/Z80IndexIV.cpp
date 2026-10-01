//===-- Z80IndexIV.cpp - Z80 Index IV Pass --------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 Index IV pass.
//
// This pass locates GEP instructions in a loop that have SCEV's of the form
// Base + Index, where Index fits within an unsigned 8-bit integer. It creates
// dedicated IVs for such indices, then rewrites the GEPs to use their zero
// extension. This allows the backend to recognize that the high byte of the
// index is zero and to use the 8-bit indexed addressing modes if appropriate.
// It then counts in 8 bits any induction variable left that needs no more.
//===----------------------------------------------------------------------===//

#include "Z80IndexIV.h"
#include "Z80InstrInfo.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#define DEBUG_TYPE "z80-indexiv"

STATISTIC(NumIndexIVs, "Number of GEPs rewritten to use an 8-bit index IV");
STATISTIC(NumNarrowedIVs, "Number of induction variables narrowed to 8 bits");

using namespace llvm;

// An induction variable that only takes values that fit in a byte needs no
// more than an 8-bit register, as long as every use of it wants no more than
// its low byte: a truncation to a byte, a zero extension, or an order or
// equality against a value that also fits in a byte, where the signed and
// unsigned orders agree. Counting in 8 bits instead, the wide variable dies
// and the backend steps and tests the counter in one register.
static bool narrowIV(Loop &L, PHINode *PN, ScalarEvolution &SE,
                     DominatorTree &DT, Type *I8) {
  BasicBlock *Latch = L.getLoopLatch();
  auto *IntTy = dyn_cast<IntegerType>(PN->getType());
  if (!Latch || !IntTy || IntTy->getBitWidth() <= 8)
    return false;
  auto *Step = dyn_cast<Instruction>(PN->getIncomingValueForBlock(Latch));
  if (!Step || Step->getOperand(0) != PN)
    return false;

  const auto FitsByte = [&](const SCEV *S) {
    return SE.getUnsignedRangeMax(S).ult(256);
  };
  const auto *AR = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(PN));
  if (!AR || AR->getLoop() != &L || !FitsByte(AR) ||
      !FitsByte(SE.getSCEV(Step)))
    return false;

  // Every use, the phi and its step aside, must want no more than a byte.
  SmallVector<Instruction *, 8> Uses;
  for (Value *V : {static_cast<Value *>(PN), static_cast<Value *>(Step)})
    for (User *U : V->users()) {
      auto *I = cast<Instruction>(U);
      if (I == PN || I == Step)
        continue;
      if (auto *Cmp = dyn_cast<ICmpInst>(I)) {
        Value *Other = Cmp->getOperand(Cmp->getOperand(0) == V ? 1 : 0);
        if (Other == PN || Other == Step ||
            !SE.isLoopInvariant(SE.getSCEV(Other), &L) ||
            !FitsByte(SE.getSCEV(Other)))
          return false;
      } else if (!(isa<TruncInst>(I) &&
                   I->getType()->getIntegerBitWidth() <= 8) &&
                 !isa<ZExtInst>(I)) {
        return false;
      }
      Uses.push_back(I);
    }
  if (Uses.empty())
    return false;

  // One narrow counter: the phi's value in a byte, reusing a narrow phi that
  // is already there, and the step taken on it rather than a second counter
  // for the stepped value.
  if (SE.getSCEV(Step) != AR->getPostIncExpr(SE))
    return false;
  SCEVExpander Rewriter(SE, "z80-indexiv");
  Rewriter.disableCanonicalMode();
  BasicBlock *Header = L.getHeader();
  Value *NarrowPN = Rewriter.expandCodeFor(SE.getTruncateExpr(AR, I8), I8,
                                           Header->getFirstInsertionPt());
  Value *NarrowStep = nullptr;
  if (auto *NP = dyn_cast<PHINode>(NarrowPN); NP && NP->getParent() == Header)
    if (auto *Inc = dyn_cast<Instruction>(NP->getIncomingValueForBlock(Latch));
        Inc && DT.dominates(Inc, Step) &&
        SE.getSCEV(Inc) == SE.getTruncateExpr(SE.getSCEV(Step), I8))
      NarrowStep = Inc;
  if (!NarrowStep)
    NarrowStep = IRBuilder<>(Step).CreateAdd(
        NarrowPN,
        Rewriter.expandCodeFor(
            SE.getTruncateExpr(AR->getStepRecurrence(SE), I8), I8, Step));

  for (Instruction *I : Uses) {
    bool OfPN =
        I->getOperand(0) == PN || (isa<ICmpInst>(I) && I->getOperand(1) == PN);
    Value *Narrow = OfPN ? NarrowPN : NarrowStep;
    IRBuilder<> Builder(I);
    Value *New;
    if (auto *Cmp = dyn_cast<ICmpInst>(I)) {
      bool WideLeft = Cmp->getOperand(0) == PN || Cmp->getOperand(0) == Step;
      Value *Other = Rewriter.expandCodeFor(
          SE.getTruncateExpr(SE.getSCEV(Cmp->getOperand(WideLeft ? 1 : 0)), I8),
          I8, I);
      // Both sides are in 0-255, where the signed order is the unsigned one.
      CmpInst::Predicate Pred =
          ICmpInst::getUnsignedPredicate(Cmp->getPredicate());
      New = WideLeft ? Builder.CreateICmp(Pred, Narrow, Other)
                     : Builder.CreateICmp(Pred, Other, Narrow);
    } else if (isa<ZExtInst>(I)) {
      New = Builder.CreateZExt(Narrow, I->getType());
    } else {
      New = Builder.CreateTrunc(Narrow, I->getType());
    }
    I->replaceAllUsesWith(New);
    I->eraseFromParent();
  }
  SE.forgetLoop(&L);
  RecursivelyDeleteDeadPHINode(PN);
  ++NumNarrowedIVs;
  return true;
}

PreservedAnalyses Z80IndexIV::run(Loop &L, LoopAnalysisManager &AM,
                                  LoopStandardAnalysisResults &AR,
                                  LPMUpdater &) {
  LLVM_DEBUG(dbgs() << "***************************** Z80 INDEX IV PASS "
                       "*****************************\n");

  auto &SE = AR.SE;
  const DataLayout &DL = L.getHeader()->getModule()->getDataLayout();

  // InRange returns whether the given range can be contained within an
  // unsigned 8-bit index.
  const auto InRange = [](const ConstantRange &Range) {
    return Range.isAllNonNegative() &&
           Range.getUpper().ule(
               APInt::getMaxValue(8).zext(Range.getBitWidth()));
  };

  Type *I8 = Type::getInt8Ty(SE.getContext());
  Type *Ptr = PointerType::get(SE.getContext(), 0);
  bool Changed = false;

  for (BasicBlock *B : L.blocks()) {
    for (auto I = B->begin(), E = B->end(); I != E; ++I) {
      // For now, only direct GEP instructions are handled, but in principle,
      // any other means of forming pointers should work as well.
      auto *GEP = dyn_cast<GetElementPtrInst>(I);
      if (!GEP)
        continue;
      LLVM_DEBUG(dbgs() << "Considering: " << *GEP << "\n");

      // Only pointer values with an additive recurrence can be made into
      // Base+Index.
      const auto *R = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(GEP));
      if (!R || R->getLoop() != &L)
        continue;
      // Only 16-bit pointer values are currently supported by this pass.
      if (R->getType()->getPointerAddressSpace() != Z80::AS_Memory)
        continue;
      LLVM_DEBUG(dbgs() << "SCEV: " << *R << "\n");

      // If the step doesn't fit in 8 bits, incrementing the index requires a
      // 16-bit add, so there's no point to the optimization.
      const auto Step = R->getStepRecurrence(SE);
      const auto StepRange = SE.getSignedRange(Step);
      if (!InRange(StepRange)) {
        LLVM_DEBUG(dbgs() << "Step range does not fit in 8 bits\n");
        LLVM_DEBUG(dbgs() << "Step: " << *Step << "\n");
        LLVM_DEBUG(dbgs() << "Range: " << StepRange << "\n");
        continue;
      }

      // The index must itself fit into 8 bits.
      const auto *Index =
          SE.getAddRecExpr(/*Start=*/SE.getConstant(R->getType(), 0), Step, &L,
                           R->getNoWrapFlags());
      const auto IndexRange = SE.getSignedRange(Index);
      if (!InRange(IndexRange)) {
        LLVM_DEBUG(dbgs() << "Index range does not fit in 8 bits\n");
        LLVM_DEBUG(dbgs() << "Index: " << *Index << "\n");
        LLVM_DEBUG(dbgs() << "Range: " << IndexRange << "\n");
        continue;
      }

      // Once the step and index are both known to fit in 8 bits, we can
      // always rewrite to a 16-bit base + 8-bit index.
      LLVM_DEBUG(dbgs() << "Rewriting to 8-bit index.\n");
      ++NumIndexIVs;
      Changed = true;

      SCEVExpander Rewriter(SE, "z80-indexiv");
      // The IVs should be computed from already available subexpressions
      // wherever possible. Canonical mode instead expands them fully to make
      // them easier to analyze.
      Rewriter.disableCanonicalMode();

      Rewriter.setInsertPoint(&*I);

      // Get a value for the 16-bit base.
      Value *BaseVal = Rewriter.expandCodeFor(R->getStart());
      // Get a value for the 8-bit index.
      Value *IndexVal = Rewriter.expandCodeFor(SE.getTruncateExpr(Index, I8));

      // Emit an "uglygep" to avoid having to find a real GEP calculation that
      // leads to the SCEV. This always works, and still preserves at least
      // some aliasing information.
      IRBuilder<> Builder(B, I);
      Value *V = Builder.CreateBitCast(BaseVal, Ptr);
      V = Builder.CreateGEP(
          I8, V, Builder.CreateZExt(IndexVal, DL.getIndexType(Ptr)), "uglygep");
      V = Builder.CreateBitCast(V, GEP->getType());

      auto Inst = I;
      --I;
      ReplaceInstWithValue(Inst, V);
    }
  }

  // The GEPs above no longer use the wide variables; narrow what is left.
  SmallVector<PHINode *, 4> Phis;
  for (PHINode &PN : L.getHeader()->phis())
    Phis.push_back(&PN);
  for (PHINode *PN : Phis)
    Changed |= narrowIV(L, PN, SE, AR.DT, I8);

  LLVM_DEBUG(dbgs() << "*****************************************************"
                       "***************************\n");
  if (!Changed)
    return PreservedAnalyses::all();
  auto PA = getLoopPassPreservedAnalyses();
  PA.preserveSet<CFGAnalyses>();
  return PA;
}
