//===-- Z80StaticFrameAlloc.cpp - Z80 Static Frame Allocation -------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Assign each static frame an offset inside one global, __static_frames.
// Then replace the placeholder target indices that frame index elimination
// wrote with the real addresses.
//
// Offsets follow the condensed call graph in topological order. Each SCC
// starts after the frames of all callers. So functions on one call chain
// never share memory, but functions that are never active together can
// share it.
//
//===----------------------------------------------------------------------===//

#include "Z80StaticFrameAlloc.h"

#include "Z80FrameLowering.h"
#include "Z80NonReentrant.h"
#include "Z80Subtarget.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "z80-static-frame-alloc"

STATISTIC(NumFrameIndicesResolved,
          "Number of static frame target indices resolved");

using namespace llvm;

namespace {

class Z80StaticFrameAlloc : public ModulePass {
public:
  static char ID;
  Z80StaticFrameAlloc() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "Z80 static frame allocation";
  }

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AU.addRequired<CallGraphWrapperPass>();
  }
};

} // namespace

// Calls that the register allocator introduced (libcalls, byte-wise runtime
// helpers) appear only in the machine code, as calls to plain symbols. When
// such a symbol resolves to a function in this module, mirror the edge into
// the call graph so the layout sees it.
static void addSymbolCallEdges(CallGraph &CG, const MachineModuleInfo &MMI) {
  Module &M = CG.getModule();
  for (auto &KV : CG) {
    CallGraphNode &CGN = *KV.second;
    if (!CGN.getFunction())
      continue;
    MachineFunction *MF = MMI.getMachineFunction(*CGN.getFunction());
    if (!MF)
      continue;
    for (const MachineBasicBlock &MBB : *MF)
      for (const MachineInstr &MI : MBB) {
        if (!MI.isCall())
          continue;
        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isSymbol())
            continue;
          GlobalValue *GV = M.getNamedValue(MO.getSymbolName());
          Function *Callee =
              GV ? dyn_cast<Function>(GV->stripPointerCastsAndAliases())
                 : nullptr;
          if (Callee && MMI.getMachineFunction(*Callee))
            CGN.addCalledFunction(nullptr, CG[Callee]);
        }
      }
  }
}

// An external call may call back into any externally-callable function.
static void addExternalEdges(CallGraph &CG) {
  assert(CG.getCallsExternalNode()->empty());
  for (auto &KV : *CG.getExternalCallingNode())
    if (KV.second->getFunction())
      CG.getCallsExternalNode()->addCalledFunction(nullptr, KV.second);
}

bool Z80StaticFrameAlloc::runOnModule(Module &M) {
  auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  auto &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  removeInlineAsmCallEdges(CG);
  addSymbolCallEdges(CG, MMI);
  addExternalEdges(CG);

  const auto StaticSizeOf = [&](const CallGraphNode *CGN) -> uint64_t {
    Function *F = CGN->getFunction();
    if (!F)
      return 0;
    MachineFunction *MF = MMI.getMachineFunction(*F);
    if (!MF)
      return 0;
    const auto &TFL = *MF->getSubtarget<Z80Subtarget>().getFrameLowering();
    return TFL.staticFrameSize(MF->getFrameInfo());
  };

  // Condense the graph into strongly-connected components.
  DenseMap<CallGraphNode *, uint64_t> SCCID;
  struct SCC {
    SmallVector<CallGraphNode *, 1> Nodes;
    uint64_t Offset = 0;
  };
  std::vector<SCC> SCCs;
  for (auto I = scc_begin(&CG), E = scc_end(&CG); I != E; ++I) {
    SCCs.emplace_back();
    for (CallGraphNode *CGN : *I) {
      SCCID[CGN] = SCCs.size() - 1;
      SCCs.back().Nodes.push_back(CGN);
    }
  }

  // Which SCCs call into each SCC. Every caller must be placed first, since
  // its frames are live while the callee's are.
  std::map<SCC *, SmallPtrSet<SCC *, 4>> CallerSCCs;
  for (SCC &CallerSCC : SCCs)
    for (CallGraphNode *CGN : CallerSCC.Nodes)
      for (const auto &CR : *CGN) {
        SCC &CalleeSCC = SCCs[SCCID[CR.second]];
        if (&CalleeSCC != &CallerSCC)
          CallerSCCs[&CalleeSCC].insert(&CallerSCC);
      }

  std::vector<SCC *> RootSCCs;
  for (SCC &S : SCCs)
    if (!CallerSCCs.count(&S))
      RootSCCs.push_back(&S);

  // Propagate offsets root-to-leaf.
  uint64_t FrameRegionSize = 0;
  while (!RootSCCs.empty()) {
    SCC &S = *RootSCCs.back();
    RootSCCs.pop_back();

    uint64_t Size = 0;
    for (CallGraphNode *CGN : S.Nodes)
      Size += StaticSizeOf(CGN);

    uint64_t End = S.Offset + Size;
    FrameRegionSize = std::max(FrameRegionSize, End);

    for (CallGraphNode *CGN : S.Nodes)
      for (const auto &CR : *CGN) {
        SCC &CalleeSCC = SCCs[SCCID[CR.second]];
        if (&CalleeSCC == &S || !CallerSCCs[&CalleeSCC].contains(&S))
          continue;
        CalleeSCC.Offset = std::max(CalleeSCC.Offset, End);
        CallerSCCs[&CalleeSCC].erase(&S);
        if (CallerSCCs[&CalleeSCC].empty())
          RootSCCs.push_back(&CalleeSCC);
      }
  }

  CallGraphNode *CallsExternal = CG.getCallsExternalNode();
  while (!CallsExternal->empty())
    CallsExternal->removeCallEdge(CallsExternal->begin());

  if (!FrameRegionSize)
    return false;

  // One block for every frame; a function's region is named after it so the
  // map file shows where each frame landed.
  Type *BlockTy =
      ArrayType::get(Type::getInt8Ty(M.getContext()), FrameRegionSize);
  auto *Frames =
      new GlobalVariable(M, BlockTy, false, GlobalValue::PrivateLinkage,
                         Constant::getNullValue(BlockTy), "__static_frames");
  // An explicit section keeps the emission on the plain label-plus-zeros
  // path; the sdas dialect has no common-symbol directives.
  Frames->setSection(".bss");
  Frames->setAlignment(Align(1));

  bool Changed = false;
  for (SCC &S : SCCs) {
    uint64_t Offset = S.Offset;
    for (CallGraphNode *CGN : S.Nodes) {
      Function *F = CGN->getFunction();
      if (!F)
        continue;
      MachineFunction *MF = MMI.getMachineFunction(*F);
      if (!MF)
        continue;
      uint64_t Size = StaticSizeOf(CGN);
      if (!Size)
        continue;

      Type *I16 = Type::getInt16Ty(M.getContext());
      Constant *Aliasee = Frames;
      if (Offset)
        Aliasee = ConstantExpr::getInBoundsPtrAdd(
            Frames, ConstantInt::get(I16, Offset));
      auto *Alias = GlobalAlias::create(
          ArrayType::get(Type::getInt8Ty(M.getContext()), Size),
          Frames->getAddressSpace(), GlobalValue::PrivateLinkage,
          Twine(F->getName()) + ".frame", Aliasee, &M);
      Offset += Size;

      for (MachineBasicBlock &MBB : *MF)
        for (MachineInstr &MI : MBB)
          for (MachineOperand &MO : MI.operands())
            if (MO.isTargetIndex()) {
              MO.ChangeToGA(Alias, MO.getOffset(), MO.getTargetFlags());
              ++NumFrameIndicesResolved;
              Changed = true;
            }
    }
  }
  return Changed;
}

char Z80StaticFrameAlloc::ID = 0;

INITIALIZE_PASS_BEGIN(Z80StaticFrameAlloc, DEBUG_TYPE,
                      "Z80 static frame allocation", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineModuleInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_END(Z80StaticFrameAlloc, DEBUG_TYPE,
                    "Z80 static frame allocation", false, false)

ModulePass *llvm::createZ80StaticFrameAllocPass() {
  return new Z80StaticFrameAlloc();
}
