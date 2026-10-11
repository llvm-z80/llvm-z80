//===-- Z80PreLegalizerCombiner.cpp
//----------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Combines on generic MachineInstrs before legalization.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "Z80Combiner.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/GlobalISel/CSEInfo.h"
#include "llvm/CodeGen/GlobalISel/Combiner.h"
#include "llvm/CodeGen/GlobalISel/CombinerHelper.h"
#include "llvm/CodeGen/GlobalISel/CombinerInfo.h"
#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutorImpl.h"
#include "llvm/CodeGen/GlobalISel/GISelValueTracking.h"
#include "llvm/CodeGen/GlobalISel/GenericMachineInstrs.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#define GET_GICOMBINER_DEPS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_DEPS

#define DEBUG_TYPE "z80-prelegalizer-combiner"

using namespace llvm;

namespace {

#define GET_GICOMBINER_TYPES
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_TYPES

// A byte sign-extended to 16 bits, possibly still folded into its load.
bool isSExtByte(Register Reg, const MachineRegisterInfo &MRI) {
  LLT S8 = LLT::scalar(8);
  const MachineInstr *Def = MRI.getVRegDef(Reg);
  if (Def->getOpcode() == TargetOpcode::G_SEXT)
    return MRI.getType(Def->getOperand(1).getReg()) == S8;
  if (const auto *Load = dyn_cast<GSExtLoad>(Def))
    return Load->getMMO().getMemoryType() == S8;
  return false;
}

// Match a 16-bit srem of two sign-extended bytes, or the low byte of such an
// sdiv taken by a truncation or a 0xFF mask, and rebuild it with G_Z80_SREM8
// or G_Z80_SDIV8.
bool matchNarrowSDivSRemOfSExt(MachineInstr &MI, MachineRegisterInfo &MRI,
                               BuildFnTy &MatchInfo) {
  if (MI.getMF()->getFunction().hasMinSize())
    return false;

  LLT S8 = LLT::scalar(8);
  LLT S16 = LLT::scalar(16);
  unsigned Opc = MI.getOpcode();
  Register Dst = MI.getOperand(0).getReg();
  MachineInstr *Div = &MI;
  if (Opc != TargetOpcode::G_SREM) {
    // The low byte of a quotient, by truncation or by a mask.
    if (Opc == TargetOpcode::G_TRUNC) {
      if (MRI.getType(Dst) != S8)
        return false;
    } else {
      auto Mask = getIConstantVRegVal(MI.getOperand(2).getReg(), MRI);
      if (!Mask || *Mask != 0xFF)
        return false;
    }
    Div = MRI.getVRegDef(MI.getOperand(1).getReg());
    if (Div->getOpcode() != TargetOpcode::G_SDIV ||
        !MRI.hasOneNonDBGUse(Div->getOperand(0).getReg()))
      return false;
  }
  if (MRI.getType(Div->getOperand(0).getReg()) != S16)
    return false;

  Register LHS = Div->getOperand(1).getReg();
  Register RHS = Div->getOperand(2).getReg();
  if (!isSExtByte(LHS, MRI) || !isSExtByte(RHS, MRI))
    return false;

  MatchInfo = [=](MachineIRBuilder &B) {
    auto L = B.buildTrunc(S8, LHS);
    auto R = B.buildTrunc(S8, RHS);
    if (Opc == TargetOpcode::G_TRUNC) {
      B.buildInstr(Z80::G_Z80_SDIV8, {Dst}, {L, R});
      return;
    }
    bool IsRem = Opc == TargetOpcode::G_SREM;
    auto Narrow =
        B.buildInstr(IsRem ? Z80::G_Z80_SREM8 : Z80::G_Z80_SDIV8, {S8}, {L, R});
    if (IsRem)
      B.buildSExt(Dst, Narrow);
    else
      B.buildZExt(Dst, Narrow);
  };
  return true;
}

class Z80PreLegalizerCombinerImpl : public Combiner {
protected:
  const CombinerHelper Helper;
  const Z80PreLegalizerCombinerImplRuleConfig &RuleConfig;
  const Z80Subtarget &STI;

public:
  Z80PreLegalizerCombinerImpl(
      MachineFunction &MF, CombinerInfo &CInfo, GISelValueTracking &VT,
      GISelCSEInfo *CSEInfo,
      const Z80PreLegalizerCombinerImplRuleConfig &RuleConfig,
      const Z80Subtarget &STI, MachineDominatorTree *MDT,
      const LegalizerInfo *LI);

  static const char *getName() { return "Z80PreLegalizerCombiner"; }

  bool tryCombineAll(MachineInstr &I) const override;

private:
#define GET_GICOMBINER_CLASS_MEMBERS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CLASS_MEMBERS
};

#define GET_GICOMBINER_IMPL
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_IMPL

Z80PreLegalizerCombinerImpl::Z80PreLegalizerCombinerImpl(
    MachineFunction &MF, CombinerInfo &CInfo, GISelValueTracking &VT,
    GISelCSEInfo *CSEInfo,
    const Z80PreLegalizerCombinerImplRuleConfig &RuleConfig,
    const Z80Subtarget &STI, MachineDominatorTree *MDT, const LegalizerInfo *LI)
    : Combiner(MF, CInfo, &VT, CSEInfo),
      Helper(Observer, B, /*IsPreLegalize*/ true, &VT, MDT, LI),
      RuleConfig(RuleConfig), STI(STI),
#define GET_GICOMBINER_CONSTRUCTOR_INITS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CONSTRUCTOR_INITS
{
}

// Pass boilerplate
class Z80PreLegalizerCombiner : public MachineFunctionPass {
public:
  static char ID;

  Z80PreLegalizerCombiner();

  StringRef getPassName() const override { return "Z80PreLegalizerCombiner"; }

  bool runOnMachineFunction(MachineFunction &MF) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  Z80PreLegalizerCombinerImplRuleConfig RuleConfig;
};
} // end anonymous namespace

void Z80PreLegalizerCombiner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  getSelectionDAGFallbackAnalysisUsage(AU);
  AU.addRequired<GISelValueTrackingAnalysisLegacy>();
  AU.addPreserved<GISelValueTrackingAnalysisLegacy>();
  AU.addRequired<MachineDominatorTreeWrapperPass>();
  AU.addPreserved<MachineDominatorTreeWrapperPass>();
  AU.addRequired<GISelCSEAnalysisWrapperPass>();
  AU.addPreserved<GISelCSEAnalysisWrapperPass>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

Z80PreLegalizerCombiner::Z80PreLegalizerCombiner() : MachineFunctionPass(ID) {
  if (!RuleConfig.parseCommandLineOption())
    report_fatal_error("Invalid rule identifier");
}

bool Z80PreLegalizerCombiner::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getProperties().hasFailedISel())
    return false;

  GISelCSEAnalysisWrapper &Wrapper =
      getAnalysis<GISelCSEAnalysisWrapperPass>().getCSEWrapper();
  auto *CSEInfo =
      &Wrapper.get(getStandardCSEConfigForOpt(MF.getTarget().getOptLevel()));

  const Z80Subtarget &ST = MF.getSubtarget<Z80Subtarget>();
  const auto *LI = ST.getLegalizerInfo();

  const Function &F = MF.getFunction();
  bool EnableOpt =
      MF.getTarget().getOptLevel() != CodeGenOptLevel::None && !skipFunction(F);
  GISelValueTracking *VT =
      &getAnalysis<GISelValueTrackingAnalysisLegacy>().get(MF);
  MachineDominatorTree *MDT =
      &getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  CombinerInfo CInfo(/*AllowIllegalOps*/ true, /*ShouldLegalizeIllegal*/ false,
                     /*LegalizerInfo*/ nullptr, EnableOpt, F.hasOptSize(),
                     F.hasMinSize());
  CInfo.MaxIterations = 1;
  CInfo.ObserverLvl = CombinerInfo::ObserverLevel::SinglePass;
  CInfo.EnableFullDCE = true;
  Z80PreLegalizerCombinerImpl Impl(MF, CInfo, *VT, CSEInfo, RuleConfig, ST, MDT,
                                   LI);
  return Impl.combineMachineInstrs();
}

char Z80PreLegalizerCombiner::ID = 0;
INITIALIZE_PASS_BEGIN(Z80PreLegalizerCombiner, DEBUG_TYPE,
                      "Combine Z80 machine instrs before legalization", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(GISelValueTrackingAnalysisLegacy)
INITIALIZE_PASS_DEPENDENCY(GISelCSEAnalysisWrapperPass)
INITIALIZE_PASS_END(Z80PreLegalizerCombiner, DEBUG_TYPE,
                    "Combine Z80 machine instrs before legalization", false,
                    false)

FunctionPass *llvm::createZ80PreLegalizerCombiner() {
  return new Z80PreLegalizerCombiner();
}
