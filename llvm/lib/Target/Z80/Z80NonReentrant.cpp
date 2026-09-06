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
// function in this module carrying the interrupt attribute; a handler
// living in foreign code must be declared through -z80-static-frames
// isr-roots so its context is modeled.
//
//===----------------------------------------------------------------------===//

#include "Z80NonReentrant.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "z80-nonreentrant"

using namespace llvm;

static cl::list<std::string> ISRRoots(
    "z80-static-frames-isr-roots", cl::CommaSeparated,
    cl::desc("Functions called from interrupt context outside this module; "
             "each roots its own execution context for the static frame "
             "analysis"));

namespace {

class Z80NonReentrantImpl {
  CallGraph &CG;
  SmallPtrSet<const CallGraphNode *, 8> Reentrant;
  SmallPtrSet<const CallGraphNode *, 16> ReachableFromCurrent;
  SmallPtrSet<const CallGraphNode *, 16> ReachableFromOther;

public:
  Z80NonReentrantImpl(CallGraph &CG) : CG(CG) {}
  bool run(Module &M);

private:
  bool isContextRoot(const Function &F) const {
    if (F.hasFnAttribute("interrupt"))
      return true;
    for (const std::string &Name : ISRRoots)
      if (F.getName() == Name)
        return true;
    return false;
  }

  void visitContext(const CallGraphNode &CGN);
};

} // namespace

static bool callsSelf(const CallGraphNode &N) {
  for (const CallGraphNode::CallRecord &CR : N)
    if (CR.second == &N)
      return true;
  return false;
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
  for (const CallGraphNode::CallRecord &CR : CGN) {
    const Function *Callee = CR.second->getFunction();
    if (Callee && isContextRoot(*Callee))
      continue;
    visitContext(*CR.second);
  }
}

bool Z80NonReentrantImpl::run(Module &M) {
  // Any external call may end up calling any externally-callable function,
  // which lets the SCC walk see recursion that passes through code outside
  // the module or through a function pointer.
  assert(CG.getCallsExternalNode()->empty());
  CG.getCallsExternalNode()->addCalledFunction(nullptr,
                                               CG.getExternalCallingNode());

  // Bottom-up recursion analysis: a single-node SCC that does not call
  // itself can never have two activations from calls alone.
  bool Changed = false;
  for (auto I = scc_begin(&CG), E = scc_end(&CG); I != E; ++I) {
    if (I->size() > 1)
      continue;
    const CallGraphNode &N = **I->begin();
    Function *F = N.getFunction();
    if (!F || F->isDeclaration() || F->doesNotRecurse() || callsSelf(N))
      continue;
    F->setDoesNotRecurse();
    Changed = true;
  }

  // Context analysis. The external world (main and any other entry called
  // from outside) is one context; each interrupt handler and each declared
  // foreign-called root is another. A function reachable from more than one
  // of them can be active twice.
  //
  // The walks run with the artificial external edge still in place, so an
  // indirect call inside one context conservatively reaches every
  // address-taken function.
  auto FinishContext = [&]() {
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

  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    if (F.doesNotRecurse() && !Reentrant.contains(CG[&F])) {
      F.addFnAttr("nonreentrant");
      Changed = true;
    }
  }
  return Changed;
}

namespace {

class Z80NonReentrant : public ModulePass {
public:
  static char ID;
  Z80NonReentrant() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "Z80 non-reentrant function analysis";
  }

  bool runOnModule(Module &M) override {
    CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
    return Z80NonReentrantImpl(CG).run(M);
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

ModulePass *llvm::createZ80NonReentrantPass() { return new Z80NonReentrant(); }
