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
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/IndVarSimplify.h"
#include "llvm/Transforms/Utils.h"

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80BranchCleanup.h"
#include "Z80CheckUnsupported.h"
#include "Z80Combiner.h"
#include "Z80DanglingDebugCleanup.h"
#include "Z80ExpandPseudo.h"
#include "Z80FixupImplicitDefs.h"
#include "Z80IndexIV.h"
#include "Z80LowerSelect.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80NonReentrant.h"
#include "Z80PostRACompareMerge.h"
#include "Z80PreEmitPeephole.h"
#include "Z80ShiftRotateChain.h"
#include "Z80SplitDjnzCounters.h"
#include "Z80StaticFrameAlloc.h"
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
  initializeZ80CheckUnsupportedPass(PR);
  initializeZ80DanglingDebugCleanupPass(PR);
  initializeZ80PreLegalizerCombinerPass(PR);
  initializeZ80PostLegalizerCombinerPass(PR);
  initializeZ80FixupImplicitDefsPass(PR);
  initializeZ80PreEmitPeepholePass(PR);
  initializeZ80LowerSelectPass(PR);
  initializeZ80ShiftRotateChainPass(PR);
  initializeZ80PostRACompareMergePass(PR);
  initializeZ80SplitDjnzCountersPass(PR);
  initializeZ80NonReentrantPass(PR);
  initializeZ80StaticFrameAllocPass(PR);
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
    "e-m:o-p:16:8-i16:8-i32:8-i64:8-i128:8-f32:8-f64:8-ve-n8:16";

// On by default for both targets. On SM83 a wide slot costs a byte more in
// static memory than on the stack, so frame lowering keeps a size build's
// wide slots on the stack; an eight-bit slot is a win either way. Explicit
// true/false overrides the default.
static cl::opt<cl::boolOrDefault> EnableStaticFramesOpt(
    "z80-static-frames",
    cl::desc("Allocate the frames of provably non-reentrant functions in "
             "static memory (default: on)"),
    cl::init(cl::boolOrDefault::BOU_UNSET), cl::Hidden);

bool Z80TargetMachine::useStaticFrames() const {
  if (getOptLevel() == CodeGenOptLevel::None)
    return false;
  switch (EnableStaticFramesOpt) {
  case cl::boolOrDefault::BOU_TRUE:
    return true;
  case cl::boolOrDefault::BOU_FALSE:
    return false;
  case cl::boolOrDefault::BOU_UNSET:
    return true;
  }
  llvm_unreachable("unhandled boolOrDefault");
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
    : CodeGenTargetMachineImpl(T, Z80DataLayout, TT, selectZ80CPU(CPU, TT), FS,
                               Options, getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      SubTarget(TT, selectZ80CPU(CPU, TT).str(), FS.str(), *this) {
  this->TLOF = std::make_unique<Z80TargetObjectFile>();

  initAsmInfo();

  setGlobalISel(true);
  // Prevents fallback to SelectionDAG by allowing direct aborts.
  setGlobalISelAbort(GlobalISelAbortMode::Enable);

  // Code size is the scarce resource here, and a repeated run of
  // instructions is worth a call. The target hook decides which functions
  // take the trade; -enable-machine-outliner still overrides both.
  this->Options.EnableMachineOutliner = true;
  this->Options.SupportsDefaultOutlining = true;
}

const Z80Subtarget *
Z80TargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  auto CPU = selectZ80CPU(CPUAttr.isValid() ? CPUAttr.getValueAsString()
                                            : StringRef(TargetCPU),
                          TargetTriple)
                 .str();
  auto FS = FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  auto &I = SubtargetMap[CPU + FS];
  if (!I) {
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
  addPass(createZ80CheckUnsupportedPass());
  addPass(createAtomicExpandLegacyPass());

  // Whole-module analysis behind the static frame allocation; runs after
  // LTO merging so the call graph covers the whole program.
  if (getZ80TargetMachine().useStaticFrames())
    addPass(createZ80NonReentrantPass(getZ80TargetMachine()));

  TargetPassConfig::addIRPasses();
  // Clean up after LSR in particular.
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createInstructionCombiningPass());
}

bool Z80PassConfig::addPreISel() { return false; }

bool Z80PassConfig::addIRTranslator() {
  addPass(new IRTranslatorLegacy(getOptLevel()));
  return false;
}

void Z80PassConfig::addPreLegalizeMachineIR() {
  if (getOptLevel() != CodeGenOptLevel::None) {
    addPass(createZ80PreLegalizerCombiner());
    addPass(createZ80ShiftRotateChainPass());
  }
}

bool Z80PassConfig::addLegalizeMachineIR() {
  addPass(new LegalizerLegacy());
  return false;
}

void Z80PassConfig::addPreRegBankSelect() {
  // Post-legalization combiner must run at all optimization levels.
  // It contains correctness rules (z80_cross_size_copy, merge_combines)
  // that are required for instruction selection to succeed.
  addPass(createZ80PostLegalizerCombiner());
  addPass(createZ80LowerSelectPass());
  addPass(createZ80DanglingDebugCleanupPass());
}

bool Z80PassConfig::addRegBankSelect() {
  addPass(new RegBankSelectLegacy());
  return false;
}

void Z80PassConfig::addPreGlobalInstructionSelect() {
  // This pass helps reduce the live ranges of constants to within a basic
  // block, which can greatly improve machine scheduling, as they can now be
  // moved around to keep register pressure low.
  addPass(new LocalizerLegacy());
}

bool Z80PassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelectLegacy());
  return false;
}

void Z80PassConfig::addOptimizedRegAlloc() {
  if (getOptLevel() != CodeGenOptLevel::None) {
    // Run the coalescer twice to coalesce RMW patterns revealed by the first
    // coalesce.
    insertPass(&llvm::TwoAddressInstructionPassID, &llvm::RegisterCoalescerID);

    // Split counter live ranges of DJNZ loops to BReg before greedy
    // regalloc so inner and sequential loop counters are assigned to B.
    insertPass(&llvm::MachineSchedulerID, createZ80SplitDjnzCountersPass());

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
  // Copy lowering leaves the halves of a pair copy declared on the wrong one
  // of the two byte moves. See Z80FixupImplicitDefs.cpp.
  addPass(createZ80FixupImplicitDefsPass());

  // Every function's frame is final past PEI, so the static frames can be
  // laid out module-wide and the placeholder operands resolved. Must run
  // before the late peepholes so they see the final operands.
  if (getZ80TargetMachine().useStaticFrames())
    addPass(createZ80StaticFrameAllocPass());

  if (getOptLevel() != CodeGenOptLevel::None) {
    addPass(createZ80PreEmitPeepholePass());

    // Remove redundant OR A / AND A when the Z flag is already valid
    // from a preceding ALU instruction.
    addPass(createZ80PostRACompareMerge());

    // The peepholes above rewrite slot accesses into register copies and leave
    // copies behind where they fold one instruction into another, so copy
    // propagation runs after them. It is told to recognise LD r,r' through
    // isCopyInstrImpl, since COPY is already lowered by this point.
    addPass(createMachineCopyPropagationPass(/*UseCopyInstr=*/true));
  }
}

void Z80PassConfig::addPreEmitPass() {
  addPass(&BranchRelaxationPassID);
  // Collapse JR_CC+JP trampolines from BranchRelaxation into JP_CC.
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createZ80BranchCleanupPass());
  // Expand pseudos that split MBBs (variable shift loops) after branch
  // relaxation. The generated JR/DJNZ branches are always short-range.
  addPass(createZ80ExpandPseudoPass());
}
