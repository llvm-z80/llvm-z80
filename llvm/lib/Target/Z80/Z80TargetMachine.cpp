//===-- Z80TargetMachine.cpp - Define TargetMachine for Z80 ---------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#include "Z80TargetMachine.h"

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/Localizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/MachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/IndVarSimplify.h"
#include "llvm/Transforms/Utils.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80BranchCleanup.h"
#include "Z80Combiner.h"
#include "Z80ExpandPseudo.h"
#include "Z80FixupImplicitDefs.h"
#include "Z80IndexIV.h"
#include "Z80LateOptimization.h"
#include "Z80LowerSelect.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80PostRAScavenging.h"
#include "Z80ShiftRotateChain.h"
#include "Z80TargetObjectFile.h"
#include "Z80TargetTransformInfo.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80Target() {
  // Register both Z80 and SM83 targets.
  RegisterTargetMachine<Z80TargetMachine> X(getTheZ80Target());
  RegisterTargetMachine<Z80TargetMachine> Y(getTheSM83Target());

  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeGlobalISel(PR);
  initializeZ80BranchCleanupPass(PR);
  initializeZ80PreLegalizerCombinerPass(PR);
  initializeZ80PostLegalizerCombinerPass(PR);
  initializeZ80FixupImplicitDefsPass(PR);
  initializeZ80LateOptimizationPass(PR);
  initializeZ80LowerSelectPass(PR);
  initializeZ80PostRAScavengingPass(PR);
  initializeZ80ShiftRotateChainPass(PR);
}

// Z80 data layout:
// e = little endian
// p:16:8 = 16-bit pointers with 8-bit alignment
// i16:8 = 16-bit integers with 8-bit alignment
// i32:8 = 32-bit integers with 8-bit alignment
// f32:8 = 32-bit floats with 8-bit alignment
// f64:8 = 64-bit floats with 8-bit alignment
// n8:16 = native integer widths are 8 and 16 bits
static const char *Z80DataLayout =
    "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-n8:16";

/// Processes a CPU name.
static StringRef getCPU(StringRef CPU, const Triple &TT) {
  if (CPU.empty() || CPU == "generic")
    return TT.getArch() == Triple::sm83 ? "sm83" : "z80";
  return CPU;
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM ? *RM : Reloc::Static;
}

Z80TargetMachine::Z80TargetMachine(const Target &T, const Triple &TT,
                                   StringRef CPU, StringRef FS,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM,
                                   std::optional<CodeModel::Model> CM,
                                   CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, Z80DataLayout, TT, getCPU(CPU, TT), FS,
                               Options, getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      SubTarget(TT, getCPU(CPU, TT).str(), FS.str(), *this) {
  this->TLOF = std::make_unique<Z80TargetObjectFile>();

  initAsmInfo();

  setGlobalISel(true);
  // Prevents fallback to SelectionDAG by allowing direct aborts.
  setGlobalISelAbort(GlobalISelAbortMode::Enable);
}

const Z80Subtarget *
Z80TargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  auto CPU = getCPU(CPUAttr.isValid() ? CPUAttr.getValueAsString()
                                      : StringRef(TargetCPU),
                    TargetTriple)
                 .str();
  auto FS = FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  auto &I = SubtargetMap[CPU + FS];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    I = std::make_unique<Z80Subtarget>(TargetTriple, CPU, FS, *this);
  }
  return I.get();
}

TargetTransformInfo
Z80TargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<Z80TTIImpl>(this, F));
}

void Z80TargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, LoopPassManager &PM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "z80-indexiv") {
          // Rewrite pointer artithmetic in loops to use 8-bit IV offsets.
          PM.addPass(Z80IndexIV());
          return true;
        }
        return false;
      });

  PB.registerLateLoopOptimizationsEPCallback(
      [](LoopPassManager &PM, OptimizationLevel Level) {
        if (Level != OptimizationLevel::O0) {
          PM.addPass(Z80IndexIV());
        }
      });
}

MachineFunctionInfo *Z80TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {

  return Z80FunctionInfo::create<Z80FunctionInfo>(
      Allocator, F, static_cast<const Z80Subtarget *>(STI));
}

//===----------------------------------------------------------------------===//
// Pass Pipeline Configuration
//===----------------------------------------------------------------------===//

namespace {
/// Z80 Code Generator Pass Configuration Options.
class Z80PassConfig : public TargetPassConfig {
public:
  Z80PassConfig(Z80TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  Z80TargetMachine &getZ80TargetMachine() const {
    return getTM<Z80TargetMachine>();
  }

  void addIRPasses() override;
  bool addPreISel() override;
  bool addIRTranslator() override;
  void addPreLegalizeMachineIR() override;
  bool addLegalizeMachineIR() override;
  void addPreRegBankSelect() override;
  bool addRegBankSelect() override;
  void addPreGlobalInstructionSelect() override;
  bool addGlobalInstructionSelect() override;

  // Register pressure is too high to work without optimized register
  // allocation.
  void addFastRegAlloc() override { addOptimizedRegAlloc(); }
  void addOptimizedRegAlloc() override;

  void addPostRewrite() override;
  void addPreSched2() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *Z80TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new Z80PassConfig(*this, PM);
}

void Z80PassConfig::addIRPasses() {
  TargetPassConfig::addIRPasses();
  // Clean up after LSR in particular.
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createInstructionCombiningPass());
}

bool Z80PassConfig::addPreISel() { return false; }

bool Z80PassConfig::addIRTranslator() {
  addPass(new IRTranslator(getOptLevel()));
  return false;
}

void Z80PassConfig::addPreLegalizeMachineIR() {
  if (getOptLevel() != CodeGenOptLevel::None) {
    addPass(createZ80PreLegalizerCombiner());
    addPass(createZ80ShiftRotateChainPass());
  }
}

bool Z80PassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer());
  return false;
}

void Z80PassConfig::addPreRegBankSelect() {
  // Post-legalization combiner must run at all optimization levels.
  // It contains correctness rules (z80_cross_size_copy, merge_combines)
  // that are required for instruction selection to succeed.
  addPass(createZ80PostLegalizerCombiner());
  addPass(createZ80LowerSelectPass());
}

bool Z80PassConfig::addRegBankSelect() {
  addPass(new RegBankSelect());
  return false;
}

void Z80PassConfig::addPreGlobalInstructionSelect() {
  // This pass helps reduce the live ranges of constants to within a basic
  // block, which can greatly improve machine scheduling, as they can now be
  // moved around to keep register pressure low.
  addPass(new Localizer());
}

bool Z80PassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelect());
  return false;
}

void Z80PassConfig::addOptimizedRegAlloc() {
  if (getOptLevel() != CodeGenOptLevel::None) {
    // Run the coalescer twice to coalesce RMW patterns revealed by the first
    // coalesce.
    insertPass(&llvm::TwoAddressInstructionPassID, &llvm::RegisterCoalescerID);

    // Re-run Live Intervals after coalescing to renumber the contained values.
    // This can allow constant rematerialization after aggressive coalescing.
    insertPass(&llvm::MachineSchedulerID, &llvm::LiveIntervalsID);
  }
  TargetPassConfig::addOptimizedRegAlloc();
}

void Z80PassConfig::addPostRewrite() {
  // Mitigation for https://github.com/llvm/llvm-project/issues/156428
  // Remove spurious super-register implicit-defs added by LiveVariables.
  // Must run before MachineCopyPropagation to prevent incorrect dead-copy
  // elimination.  See Z80FixupImplicitDefs.cpp for full explanation.
  addPass(createZ80FixupImplicitDefsPass());
}

void Z80PassConfig::addPreSched2() {
  // Lower control flow pseudos.
  addPass(&FinalizeISelID);
  // Lower pseudos produced by control flow pseudos.
  addPass(&ExpandPostRAPseudosID);
  addPass(createZ80PostRAScavengingPass());

  // This is currently mandatory, since it lowers CMPTermZ.
  addPass(createZ80LateOptimizationPass());
}

void Z80PassConfig::addPreEmitPass() {
  addPass(&BranchRelaxationPassID);
  // Collapse JR_CC+JP trampolines from BranchRelaxation into JP_CC.
  addPass(createZ80BranchCleanupPass());
  // Expand pseudos that split MBBs (variable shift loops) after branch
  // relaxation. The generated JR/DJNZ branches are always short-range.
  addPass(createZ80ExpandPseudoPass());
}
