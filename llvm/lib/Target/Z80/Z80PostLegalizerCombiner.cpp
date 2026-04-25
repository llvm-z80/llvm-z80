//===-- Z80PostLegalizerCombiner.cpp
//---------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-legalization combines on generic MachineInstrs.
// The combines here must preserve instruction legality.
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
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#define GET_GICOMBINER_DEPS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_DEPS

#define DEBUG_TYPE "z80-postlegalizer-combiner"

using namespace llvm;

namespace {

#define GET_GICOMBINER_TYPES
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_TYPES

// Match cross-size COPYs between virtual registers of different sizes.
// The legalizer's narrowScalar can produce COPYs like s16→s8 which should
// be G_TRUNC, or s8→s16 which should be G_ANYEXT. ISel constrains both
// sides of a COPY to the same register class, causing conflicts.
bool matchCrossSizeCopy(MachineInstr &MI, MachineRegisterInfo &MRI,
                        unsigned &NewOpc) {
  if (MI.getOpcode() != TargetOpcode::COPY)
    return false;

  Register DstReg = MI.getOperand(0).getReg();
  Register SrcReg = MI.getOperand(1).getReg();

  if (!DstReg.isVirtual() || !SrcReg.isVirtual())
    return false;

  LLT DstTy = MRI.getType(DstReg);
  LLT SrcTy = MRI.getType(SrcReg);
  if (!DstTy.isValid() || !SrcTy.isValid())
    return false;

  unsigned DstSize = DstTy.getSizeInBits();
  unsigned SrcSize = SrcTy.getSizeInBits();
  if (DstSize == SrcSize)
    return false;

  NewOpc = (DstSize < SrcSize) ? TargetOpcode::G_TRUNC : TargetOpcode::G_ANYEXT;
  return true;
}

void applyCrossSizeCopy(MachineInstr &MI, unsigned &NewOpc) {
  const TargetInstrInfo &TII = *MI.getMF()->getSubtarget().getInstrInfo();
  MI.setDesc(TII.get(NewOpc));
}

class Z80PostLegalizerCombinerImpl : public Combiner {
protected:
  const CombinerHelper Helper;
  const Z80PostLegalizerCombinerImplRuleConfig &RuleConfig;
  const Z80Subtarget &STI;

public:
  Z80PostLegalizerCombinerImpl(
      MachineFunction &MF, CombinerInfo &CInfo, GISelValueTracking &VT,
      GISelCSEInfo *CSEInfo,
      const Z80PostLegalizerCombinerImplRuleConfig &RuleConfig,
      const Z80Subtarget &STI, MachineDominatorTree *MDT,
      const LegalizerInfo *LI);

  static const char *getName() { return "Z80PostLegalizerCombiner"; }

  bool tryCombineAll(MachineInstr &I) const override;

private:
#define GET_GICOMBINER_CLASS_MEMBERS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CLASS_MEMBERS
};

#define GET_GICOMBINER_IMPL
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_IMPL

Z80PostLegalizerCombinerImpl::Z80PostLegalizerCombinerImpl(
    MachineFunction &MF, CombinerInfo &CInfo, GISelValueTracking &VT,
    GISelCSEInfo *CSEInfo,
    const Z80PostLegalizerCombinerImplRuleConfig &RuleConfig,
    const Z80Subtarget &STI, MachineDominatorTree *MDT, const LegalizerInfo *LI)
    : Combiner(MF, CInfo, &VT, CSEInfo),
      Helper(Observer, B, /*IsPreLegalize*/ false, &VT, MDT, LI),
      RuleConfig(RuleConfig), STI(STI),
#define GET_GICOMBINER_CONSTRUCTOR_INITS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CONSTRUCTOR_INITS
{
}

class Z80PostLegalizerCombiner : public MachineFunctionPass {
public:
  static char ID;

  Z80PostLegalizerCombiner();

  StringRef getPassName() const override { return "Z80PostLegalizerCombiner"; }

  bool runOnMachineFunction(MachineFunction &MF) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  Z80PostLegalizerCombinerImplRuleConfig RuleConfig;
};
} // end anonymous namespace

void Z80PostLegalizerCombiner::getAnalysisUsage(AnalysisUsage &AU) const {
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

Z80PostLegalizerCombiner::Z80PostLegalizerCombiner() : MachineFunctionPass(ID) {
  if (!RuleConfig.parseCommandLineOption())
    report_fatal_error("Invalid rule identifier");
}

bool Z80PostLegalizerCombiner::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getProperties().hasFailedISel())
    return false;
  assert(MF.getProperties().hasLegalized() && "Expected a legalized function?");
  const Function &F = MF.getFunction();
  bool EnableOpt =
      MF.getTarget().getOptLevel() != CodeGenOptLevel::None && !skipFunction(F);

  const Z80Subtarget &ST = MF.getSubtarget<Z80Subtarget>();
  const auto *LI = ST.getLegalizerInfo();

  GISelValueTracking *VT =
      &getAnalysis<GISelValueTrackingAnalysisLegacy>().get(MF);
  MachineDominatorTree *MDT =
      &getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  GISelCSEAnalysisWrapper &Wrapper =
      getAnalysis<GISelCSEAnalysisWrapperPass>().getCSEWrapper();
  auto *CSEInfo =
      &Wrapper.get(getStandardCSEConfigForOpt(MF.getTarget().getOptLevel()));

  CombinerInfo CInfo(/*AllowIllegalOps*/ true, /*ShouldLegalizeIllegal*/ false,
                     /*LegalizerInfo*/ nullptr, EnableOpt, F.hasOptSize(),
                     F.hasMinSize());
  Z80PostLegalizerCombinerImpl Impl(MF, CInfo, *VT, CSEInfo, RuleConfig, ST,
                                    MDT, LI);
  return Impl.combineMachineInstrs();
}

char Z80PostLegalizerCombiner::ID = 0;
INITIALIZE_PASS_BEGIN(Z80PostLegalizerCombiner, DEBUG_TYPE,
                      "Combine Z80 MachineInstrs after legalization", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(GISelValueTrackingAnalysisLegacy)
INITIALIZE_PASS_DEPENDENCY(GISelCSEAnalysisWrapperPass)
INITIALIZE_PASS_END(Z80PostLegalizerCombiner, DEBUG_TYPE,
                    "Combine Z80 MachineInstrs after legalization", false,
                    false)

FunctionPass *llvm::createZ80PostLegalizerCombiner() {
  return new Z80PostLegalizerCombiner();
}
