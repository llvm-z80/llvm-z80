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
#include "Z80Combiner.h"
#include "Z80ExpandPseudo.h"
#include "Z80FixupImplicitDefs.h"
#include "Z80IndexIV.h"
#include "Z80LateOptimization.h"
#include "Z80LowerSelect.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80NonReentrant.h"
#include "Z80PostRACompareMerge.h"
#include "Z80PostRAScavenging.h"
#include "Z80ShiftRotateChain.h"
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
  initializeZ80PreLegalizerCombinerPass(PR);
  initializeZ80PostLegalizerCombinerPass(PR);
  initializeZ80FixupImplicitDefsPass(PR);
  initializeZ80LateOptimizationPass(PR);
  initializeZ80LowerSelectPass(PR);
  initializeZ80PostRAScavengingPass(PR);
  initializeZ80ShiftRotateChainPass(PR);
  initializeZ80PostRACompareMergePass(PR);
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

/// Processes a CPU name.
static StringRef getCPU(StringRef CPU, const Triple &TT) {
  if (CPU.empty() || CPU == "generic")
    return TT.getArch() == Triple::sm83 ? "sm83" : "z80";
  return CPU;
}

// Default is on for Z80 and off for SM83, whose lack of a direct absolute
// load makes the trade unprofitable until the frames can go in the high
// page. Explicit true/false overrides the per-target default.
static cl::opt<cl::boolOrDefault> EnableStaticFramesOpt(
    "z80-static-frames",
    cl::desc("Allocate the frames of provably non-reentrant functions in "
             "static memory (default: on for z80, off for sm83)"),
    cl::init(cl::boolOrDefault::BOU_UNSET), cl::Hidden);

static bool useStaticFrames(const Z80TargetMachine &TM) {
  switch (EnableStaticFramesOpt) {
  case cl::boolOrDefault::BOU_TRUE:
    return true;
  case cl::boolOrDefault::BOU_FALSE:
    return false;
  case cl::boolOrDefault::BOU_UNSET:
    return TM.getTargetTriple().getArch() != Triple::sm83;
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
    : CodeGenTargetMachineImpl(T, Z80DataLayout, TT, getCPU(CPU, TT), FS,
                               Options, getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      SubTarget(TT, getCPU(CPU, TT).str(), FS.str(), *this) {
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

  auto CPU = getCPU(CPUAttr.isValid() ? CPUAttr.getValueAsString()
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
    Type *NewRet = NewRetTypes.empty()      ? Type::getVoidTy(Ctx)
                   : NewRetTypes.size() == 1 ? NewRetTypes[0]
                                             : StructType::get(Ctx, NewRetTypes);
    SmallVector<Type *, 8> ParamTys;
    for (Value *V : NewArgs)
      ParamTys.push_back(V->getType());
    FunctionType *NewFTy = FunctionType::get(NewRet, ParamTys, false);
    InlineAsm *NewIA =
        InlineAsm::get(NewFTy, IA->getAsmString(), NewConstraints,
                       IA->hasSideEffects(), IA->isAlignStack(),
                       IA->getDialect(), IA->canThrow());
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

// The legalizer can erase the last definition of a wide value (a scalarized
// vector, a split integer) while a debug operand still refers to it.
// RegBankSelect maps debug operands like any other, and no 8/16-bit bank can
// carry the wide dangling type. The value no longer exists, so mark it as
// unavailable instead.
class Z80DanglingDebugCleanup : public MachineFunctionPass {
public:
  static char ID;
  Z80DanglingDebugCleanup() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override {
    return "Z80 dangling debug value cleanup";
  }
  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineRegisterInfo &MRI = MF.getRegInfo();
    bool Changed = false;
    auto IsDangling = [&](const MachineOperand &MO) {
      return MO.isReg() && MO.getReg().isVirtual() &&
             !MRI.getVRegDef(MO.getReg());
    };
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB) {
        if (!MI.isDebugInstr() || none_of(MI.debug_operands(), IsDangling))
          continue;
        // A location with an unavailable operand cannot be evaluated at all,
        // so the canonical form marks the whole value undef.
        if (MI.isDebugValue())
          MI.setDebugValueUndef();
        else
          for (MachineOperand &MO : MI.debug_operands())
            if (IsDangling(MO))
              MO.setReg(Register());
        Changed = true;
      }
    return Changed;
  }
};
char Z80DanglingDebugCleanup::ID = 0;

class Z80CheckUnsupported : public FunctionPass {
public:
  static char ID;
  Z80CheckUnsupported() : FunctionPass(ID) {}
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
char Z80CheckUnsupported::ID = 0;
} // namespace

void Z80PassConfig::addIRPasses() {
  addPass(new Z80CheckUnsupported());
  addPass(createAtomicExpandLegacyPass());

  // Whole-module analysis behind the static frame allocation; runs after
  // LTO merging so the call graph covers the whole program.
  if (useStaticFrames(getZ80TargetMachine()) &&
      getOptLevel() != CodeGenOptLevel::None)
    addPass(createZ80NonReentrantPass());

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
  addPass(new Z80DanglingDebugCleanup());
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
  addPass(createZ80PostRAScavengingPass());

  // Every function's frame is final past PEI, so the static frames can be
  // laid out module-wide and the placeholder operands resolved. Must run
  // before the late peepholes so they see the final operands.
  if (useStaticFrames(getZ80TargetMachine()) &&
      getOptLevel() != CodeGenOptLevel::None)
    addPass(createZ80StaticFrameAllocPass());

  // This is currently mandatory, since it lowers CMPTermZ.
  addPass(createZ80LateOptimizationPass());

  // Remove redundant OR A / AND A when the Z flag is already valid
  // from a preceding ALU instruction.
  addPass(createZ80PostRACompareMerge());

  // The peepholes above rewrite slot accesses into register copies and leave
  // copies behind where they fold one instruction into another, so copy
  // propagation runs after them. It is told to recognise LD r,r' through
  // isCopyInstrImpl, since COPY is already lowered by this point.
  addPass(createMachineCopyPropagationPass(/*UseCopyInstr=*/true));
}

void Z80PassConfig::addPreEmitPass() {
  addPass(&BranchRelaxationPassID);
  // Collapse JR_CC+JP trampolines from BranchRelaxation into JP_CC.
  addPass(createZ80BranchCleanupPass());
  // Expand pseudos that split MBBs (variable shift loops) after branch
  // relaxation. The generated JR/DJNZ branches are always short-range.
  addPass(createZ80ExpandPseudoPass());
}
