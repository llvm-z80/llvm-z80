//===-- Z80NonReentrant.cpp - Z80 NonReentrant Pass -----------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A function whose frame can live at a fixed address must satisfy two
// independent conditions, both derived from the module call graph:
//
//  * It never recurses: no cycle reaches it, including cycles that pass
//    through external code or an indirect call, which the graph models
//    conservatively as calls to every address-taken function.
//
//  * At most one execution context reaches it. Interrupt handlers run
//    concurrently with everything else, so each handler roots its own
//    context and the external world (main and any other outside entry)
//    roots another; a function reachable from two contexts can have two
//    activations live at once and keeps its stack frame.
//
// The result is the "nonreentrant" attribute. Everything downstream keys
// off that attribute alone, so the analysis can be replaced without
// touching the frame machinery.
//
// Enabling static frames asserts that every interrupt entry point is a
// function in this module carrying the interrupt attribute. A function
// entered from a context the analysis cannot see, such as a handler living
// in foreign code, must carry the no-static-frame target feature (spelled
// __attribute__((target("no-static-frame"))) in C); it and everything it
// reaches then keep their stack frames.
//
//===----------------------------------------------------------------------===//

#include "Z80NonReentrant.h"

#include "Z80Subtarget.h"
#include "Z80TargetMachine.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/LTO/LTO.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-nonreentrant"

STATISTIC(NumDoesNotRecurse, "Number of functions proved not to recurse");
STATISTIC(NumNonReentrant, "Number of functions marked nonreentrant");

using namespace llvm;

// A freestanding module (per the C standard: __STDC_HOSTED__ == 0) has no
// hosted C library and no code outside the module that can call back in.
// clang emits the "Freestanding" module flag under -ffreestanding
// (CodeGenModule.cpp: if (LangOpts.Freestanding) addModuleFlag(...)). Using
// the standard flag rather than a fork-local switch keeps this consistent
// with every other target and toolchain — bare-metal / firmware / kernel
// builds already use -ffreestanding, so the closed-world proof lights up
// automatically.
static bool isFreestandingModule(const Module &M) {
  if (const auto *Flag = mdconst::extract_or_null<ConstantInt>(
          M.getModuleFlag("Freestanding")))
    return Flag->getZExtValue() != 0;
  return false;
}

namespace {

class Z80NonReentrantImpl {
  const Z80TargetMachine &TM;
  CallGraph &CG;
  SmallPtrSet<const CallGraphNode *, 8> Reentrant;
  SmallPtrSet<const CallGraphNode *, 16> ReachableFromCurrent;
  SmallPtrSet<const CallGraphNode *, 16> ReachableFromOther;

public:
  Z80NonReentrantImpl(const Z80TargetMachine &TM, CallGraph &CG)
      : TM(TM), CG(CG) {}
  bool run(Module &M);

private:
  bool isContextRoot(const Function &F) const {
    return F.hasFnAttribute("interrupt");
  }

  void markReentrantReachable(const CallGraphNode &CGN);
  void visitContext(const CallGraphNode &CGN);
};

} // namespace

static bool callsSelf(const CallGraphNode &N) {
  for (const CallGraphNode::CallRecord &CR : N)
    if (CR.second == &N)
      return true;
  return false;
}

// WHAT: Identify call-graph records that represent inline assembly rather
// than true function calls or indirect calls through function pointers.
//
// WHY: Stock LLVM's generic CallGraph builder routes any call where
// Call->getCalledFunction() is null to CallsExternalNode. This lumps
// indirect calls through function pointers together with inline assembly.
// In this pass, CallsExternalNode connects to ExternalCallingNode, so
// treating inline asm as an external edge would falsely treat an asm
// statement as a potential indirect callback into the module.
//
// GOTCHA: Reference/synthetic edges (such as CallsExternalNode ->
// ExternalCallingNode) have CR.first == std::nullopt. Only call records
// with a genuine instruction can be inline assembly.
//
// EXAMPLE: In firmware (e.g. autoload-in-c/rom.c), an ISR epilogue
// contains `__asm__("ei")`, lowered to `call void asm sideeffect "ei",
// ""()`. Without this check:
//   isr -> CallsExternalNode -> ExternalCallingNode -> {compare_6bytes...}
// This falsely marked `compare_6bytes` (a pure leaf) as reachable from
// both main and the ISR context, stripping its static frame and adding
// 39 `add hl,sp` instructions. With this filter, the edge is ignored.
static bool isInlineAsmEdge(const CallGraphNode::CallRecord &CR) {
  if (!CR.first)
    return false; // synthetic edges have no instruction; cannot be asm
  auto *CB = dyn_cast_or_null<CallBase>(*CR.first);
  return CB && CB->isInlineAsm();
}

// WHAT: Identify call-graph records that represent synthetic edges out of
// external declarations to CallsExternalNode.
//
// WHY: Stock LLVM's CallGraph adds a synthetic edge (nullptr -> CallsExternalNode)
// for any external function declaration (e.g. port I/O, BIOS routines,
// compiler-rt math helpers). In context reachability analysis, this synthetic
// edge must not be traversed: a direct call from an ISR to a declared helper
// does not spontaneously call back into the module's mainline functions.
// In contrast, genuine indirect calls (through function pointers) have
// CR.first pointing to the call instruction and continue to be conservatively
// routed to CallsExternalNode -> ExternalCallingNode.
static bool isExternalDeclarationEdge(const CallGraphNode &Caller,
                                      const CallGraphNode::CallRecord &CR) {
  const Function *F = Caller.getFunction();
  return F && F->isDeclaration() && !CR.first;
}

// A function entered from a context the module analysis cannot see keeps
// its stack frame, and so must everything it can reach: any of it may run
// concurrently with any other context. The walk happens with the
// artificial external edge in place, so an indirect or external call in
// the tree conservatively spreads to every externally-callable function.
// Inline asm is excluded: it cannot call back into the module at all.
void Z80NonReentrantImpl::markReentrantReachable(const CallGraphNode &CGN) {
  if (!Reentrant.insert(&CGN).second)
    return;
  LLVM_DEBUG({
    if (const Function *F = CGN.getFunction())
      dbgs() << "Reachable from a foreign context: " << F->getName() << "\n";
  });
  for (const CallGraphNode::CallRecord &CR : CGN) {
    if (isInlineAsmEdge(CR) || isExternalDeclarationEdge(CGN, CR))
      continue; // inline asm and external declarations do not call into module
    markReentrantReachable(*CR.second);
  }
}

void Z80NonReentrantImpl::visitContext(const CallGraphNode &CGN) {
  if (!ReachableFromCurrent.insert(&CGN).second)
    return;

  const Function *F = CGN.getFunction();
  if (F && !F->isDeclaration() && ReachableFromOther.contains(&CGN)) {
    LLVM_DEBUG(dbgs() << "Reachable from multiple contexts: " << F->getName()
                      << "\n");
    Reentrant.insert(&CGN);
  }
  // A context does not extend into another context's root: an interrupt
  // handler reached from here still only ever runs in its own context.
  // Inline asm is excluded (see isInlineAsmEdge): it cannot call back into
  // the module, so it must not carry this context out to every
  // externally-callable function via CallsExternalNode.
  for (const CallGraphNode::CallRecord &CR : CGN) {
    if (isInlineAsmEdge(CR) || isExternalDeclarationEdge(CGN, CR))
      continue; // inline asm and external declarations do not call into module
    const Function *Callee = CR.second->getFunction();
    if (Callee && isContextRoot(*Callee))
      continue; // do not cross into an independent interrupt context
    visitContext(*CR.second);
  }
}

bool Z80NonReentrantImpl::run(Module &M) {
  // This pass is the attribute's only legitimate writer; drop any that
  // arrived with the input IR so everything downstream is backed by this
  // run's analysis.
  bool Changed = false;
  for (Function &F : M.functions())
    if (F.hasFnAttribute("nonreentrant")) {
      F.removeFnAttr("nonreentrant");
      Changed = true;
    }

  // Any external call may end up calling any externally-callable function,
  // which lets the SCC walk see recursion that passes through code outside
  // the module or through a function pointer.
  assert(CG.getCallsExternalNode()->empty());
  if (isFreestandingModule(M)) {
    // In a freestanding / closed-world module (e.g. firmware ROM or OS kernel),
    // foreign code outside the module does not exist, so external calls cannot
    // re-enter arbitrary module functions. Indirect calls through function
    // pointers still conservatively reach any locally address-taken function.
    for (Function &F : M.functions()) {
      if (!F.isDeclaration() && F.hasAddressTaken())
        CG.getCallsExternalNode()->addCalledFunction(nullptr, CG[&F]);
    }
  } else {
    // In an open-world library under separate compilation, any external call
    // may end up calling any externally-callable function, which lets the SCC
    // walk see recursion that passes through code outside the module.
    CG.getCallsExternalNode()->addCalledFunction(nullptr,
                                                 CG.getExternalCallingNode());
  }

  // Operations like block copies and wide arithmetic only become calls
  // during instruction selection, so their callees have no edge in the IR
  // call graph; the layout pass reads such calls off the machine code, but
  // this analysis runs before any exists. When one of those runtime
  // functions is defined in this module, treat it as callable from every
  // context, so that with a second context around it and everything it
  // calls stay on the stack.
  SmallVector<CallGraphNode *, 8> ImplicitCallees;
  for (const char *Name :
       lto::LTO::getRuntimeLibcallSymbols(M.getTargetTriple())) {
    Function *F = M.getFunction(Name);
    if (F && !F->isDeclaration())
      ImplicitCallees.push_back(CG[F]);
  }

  // A function that cleared the static-frame feature (the no-static-frame
  // spelling of the target attribute) is entered from a context this module
  // cannot see; it and its whole call tree stay on the stack. The IR walk
  // does not see the calls instruction selection will introduce, so the
  // runtime functions above count as part of any such tree.
  bool HasForeignContext = false;
  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    if (!TM.getSubtargetImpl(F)->hasStaticFrame()) {
      HasForeignContext = true;
      markReentrantReachable(*CG[&F]);
    }
  }
  if (HasForeignContext)
    for (CallGraphNode *N : ImplicitCallees)
      markReentrantReachable(*N);

  // Bottom-up recursion analysis: a single-node SCC that does not call
  // itself can never have two activations from calls alone. The walk covers
  // the nodes reachable from outside; everything else is remembered so the
  // final marking can leave it alone.
  SmallPtrSet<const CallGraphNode *, 32> Analyzed;
  for (auto I = scc_begin(&CG), E = scc_end(&CG); I != E; ++I) {
    Analyzed.insert(I->begin(), I->end());
    if (I->size() > 1)
      continue;
    const CallGraphNode &N = **I->begin();
    Function *F = N.getFunction();
    if (!F || F->isDeclaration() || F->doesNotRecurse() || callsSelf(N))
      continue;
    F->setDoesNotRecurse();
    ++NumDoesNotRecurse;
    Changed = true;
  }

  // Context analysis. The external world (main and any other entry called
  // from outside) is one context; each interrupt handler is another. A
  // function reachable from more than one of them can be active twice.
  //
  // The walks run with the artificial external edge still in place, so an
  // indirect call inside one context conservatively reaches every
  // address-taken or externally-callable function. Synthetic edges from
  // external function declarations are skipped (see isExternalDeclarationEdge).
  auto FinishContext = [&]() {
    for (CallGraphNode *N : ImplicitCallees)
      visitContext(*N);
    ReachableFromOther.insert(ReachableFromCurrent.begin(),
                              ReachableFromCurrent.end());
    ReachableFromCurrent.clear();
  };

  visitContext(*CG.getExternalCallingNode());
  FinishContext();
  for (Function &F : M.functions()) {
    if (F.isDeclaration() || !isContextRoot(F))
      continue;
    visitContext(*CG[&F]);
    FinishContext();
  }

  CG.getCallsExternalNode()->removeAllCalledFunctions();

  // A function the recursion walk never reached (internal, no callers, its
  // address never taken) keeps its stack frame even when the input IR calls
  // it norecurse: the layout pass walks the same reachable graph, so a
  // frame it cannot see must not exist.
  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    if (Analyzed.contains(CG[&F]) && F.doesNotRecurse() &&
        !Reentrant.contains(CG[&F])) {
      F.addFnAttr("nonreentrant");
      ++NumNonReentrant;
      Changed = true;
    }
  }
  return Changed;
}

namespace {

class Z80NonReentrant : public ModulePass {
  const Z80TargetMachine *TM = nullptr;

public:
  static char ID;
  Z80NonReentrant() : ModulePass(ID) {}
  Z80NonReentrant(const Z80TargetMachine &TM) : ModulePass(ID), TM(&TM) {}

  StringRef getPassName() const override {
    return "Z80 non-reentrant function analysis";
  }

  bool runOnModule(Module &M) override {
    if (!TM)
      return false;
    CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
    return Z80NonReentrantImpl(*TM, CG).run(M);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<CallGraphWrapperPass>();
  }
};

} // namespace

char Z80NonReentrant::ID = 0;

INITIALIZE_PASS_BEGIN(Z80NonReentrant, DEBUG_TYPE,
                      "Z80 non-reentrant function analysis", false, false)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_END(Z80NonReentrant, DEBUG_TYPE,
                    "Z80 non-reentrant function analysis", false, false)

ModulePass *llvm::createZ80NonReentrantPass(const Z80TargetMachine &TM) {
  return new Z80NonReentrant(TM);
}
