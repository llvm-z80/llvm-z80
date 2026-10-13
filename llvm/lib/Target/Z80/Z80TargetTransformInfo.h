//===- Z80TargetTransformInfo.h - Z80 specific TTI --------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a TargetTransformInfo::Concept conforming object specific
// to the Z80 target machine. It uses the target's detailed information to
// provide more precise answers to certain TTI queries, while letting the
// target-independent and default TTI implementations handle the rest.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_Z80_Z80TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_Z80_Z80TARGETTRANSFORMINFO_H

#include "Z80TargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Support/BranchProbability.h"

namespace llvm {

class Z80TTIImpl : public BasicTTIImplBase<Z80TTIImpl> {
  using BaseT = BasicTTIImplBase<Z80TTIImpl>;

  friend BaseT;

  const Z80Subtarget *ST;
  const Z80TargetLowering *TLI;

  const Z80Subtarget *getST() const { return ST; }
  const Z80TargetLowering *getTLI() const { return TLI; }

public:
  explicit Z80TTIImpl(const Z80TargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  // All div, rem, and divrem ops are libcalls, so any possible combination
  // exists.
  bool hasDivRemOp(Type *DataType, bool IsSigned) const override {
    return true;
  }

  bool isLSRCostLess(const TargetTransformInfo::LSRCost &C1,
                     const TargetTransformInfo::LSRCost &C2) const override {
    // Prefer instruction count to the other metrics.
    return std::tie(C1.Insns, C1.NumRegs, C1.AddRecCost, C1.NumIVMuls,
                    C1.NumBaseAdds, C1.ScaleCost, C1.ImmCost, C1.SetupCost) <
           std::tie(C2.Insns, C2.NumRegs, C2.AddRecCost, C2.NumIVMuls,
                    C2.NumBaseAdds, C2.ScaleCost, C2.ImmCost, C2.SetupCost);
  }

  // Registers are scarce, so computing a select operand before the branch is
  // expensive even for cheap arithmetic.
  bool isExpensiveToSpeculativelyExecute(const Instruction *I) const override {
    auto *BO = dyn_cast<BinaryOperator>(I);
    if (!BO || !I->getType()->isIntegerTy()) {
      // 1. A compare narrows values, so computing the compare before the
      //    branch is usually beneficial.
      auto IsBool = [](const Value *V) {
        return V->getType()->getScalarType()->isIntegerTy(1);
      };
      if (IsBool(I))
        return false;
      // 2. Keep a cast turning a compare result into a number before the
      //    branch.
      if (isa<CastInst>(I) && IsBool(I->getOperand(0)))
        return false;
      // 3. Leave an instruction doing nothing in place.
      if (isa<FreezeInst, BitCastInst, AddrSpaceCastInst, PtrToIntInst,
              IntToPtrInst, ExtractValueInst, InsertValueInst>(I))
        return false;
      // 4. Optimize an operation outside 1 to 3 by running the operation
      //    after the branch.
      return true;
    }
    // 5. Computing integer arithmetic before the branch raises register
    //    pressure, so move the arithmetic after the branch.
    auto *Sel =
        I->hasOneUse() ? dyn_cast<SelectInst>(I->user_back()) : nullptr;
    if (!Sel)
      return true;
    // Returns the value providing the byte of V at bit Lo. The byte comes
    // unchanged from the first operand of
    //   - xor or or with a constant zero byte there,
    //   - and with a constant all-ones byte there,
    //   - add or sub with a constant zero there and in every byte below,
    // so the search continues there. Otherwise V itself provides the byte.
    auto ByteSource = [](const Value *V, unsigned Lo) {
      while (auto *Op = dyn_cast<BinaryOperator>(V)) {
        const APInt *C;
        if (!PatternMatch::match(Op->getOperand(1), PatternMatch::m_APInt(C)))
          break;
        unsigned Width = std::min(8u, C->getBitWidth() - Lo);
        APInt Byte = C->extractBits(Width, Lo);
        bool Unchanged;
        switch (Op->getOpcode()) {
        case Instruction::Xor:
        case Instruction::Or:
          Unchanged = Byte.isZero();
          break;
        case Instruction::And:
          Unchanged = Byte.isAllOnes();
          break;
        case Instruction::Add:
        case Instruction::Sub:
          Unchanged = C->extractBits(Lo + Width, 0).isZero();
          break;
        default:
          Unchanged = false;
          break;
        }
        if (!Unchanged)
          break;
        V = Op->getOperand(0);
      }
      return V;
    };
    // 6. When both arms share a byte, computing the arithmetic before the
    //    branch is beneficial, so keep the arithmetic in place.
    const Value *Other =
        Sel->getTrueValue() == I ? Sel->getFalseValue() : Sel->getTrueValue();
    for (unsigned Lo = 0; Lo < I->getType()->getIntegerBitWidth(); Lo += 8)
      if (ByteSource(I, Lo) == ByteSource(Other, Lo))
        return false;
    // 7. Move integer arithmetic with no shared byte after the branch.
    return true;
  }

  BranchProbability getPredictableBranchThreshold() const override {
    return BranchProbability(0, 1);
  }

  bool isValidAddrSpaceCast(unsigned FromAS, unsigned ToAS) const override {
    return true;
  }

  // Z80 has only 3 GP register pairs (BC, DE, HL). Inlining large functions
  // causes massive register spilling that dwarfs the benefit.
  // Allow inlining for:
  //   - functions marked inlinehint (e.g. Rust iterators)
  //   - small functions (≤ 10 instructions) where call overhead dominates
  bool areInlineCompatible(const Function *Caller,
                           const Function *Callee) const override {
    if (Callee->hasFnAttribute(Attribute::InlineHint))
      return true;
    if (Callee->getInstructionCount() <= 10)
      return true;
    return false;
  }
};

} // end namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_Z80TARGETTRANSFORMINFO_H
