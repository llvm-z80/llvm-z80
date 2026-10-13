//===-- Z80LowerThreadLocal.cpp - Z80 thread-local variable lowering ------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Z80 has no thread pointer and no TLS ABI, so a program is a single
// thread and thread-local variables become ordinary globals.
//
// If you need a copy per thread, compile with -femulated-tls and provide
// __emutls_get_address yourself. Every access then calls
// __emutls_get_address.
//
// (I don't know if this feature would be needed, but... who knows? Someone
// might want to create an RTOS on the Z80.)
//
//===----------------------------------------------------------------------===//

#include "Z80LowerThreadLocal.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Target/TargetMachine.h"

#define DEBUG_TYPE "z80-lower-thread-local"

using namespace llvm;

namespace {

class Z80LowerThreadLocal : public ModulePass {
  const TargetMachine *TM = nullptr;

public:
  static char ID;
  Z80LowerThreadLocal() : ModulePass(ID) {}
  Z80LowerThreadLocal(const TargetMachine &TM) : ModulePass(ID), TM(&TM) {}

  StringRef getPassName() const override {
    return "Z80 thread-local variable lowering";
  }

  bool runOnModule(Module &M) override;
};

} // namespace

bool Z80LowerThreadLocal::runOnModule(Module &M) {
  const bool Emulated = TM && TM->useEmulatedTLS();

  SmallVector<GlobalValue *, 4> ThreadLocals;
  for (GlobalValue &GV : M.global_values())
    if (GV.isThreadLocal())
      ThreadLocals.push_back(&GV);
  if (ThreadLocals.empty())
    return false;

  FunctionCallee GetAddress;
  bool Changed = false;
  for (GlobalValue *GV : ThreadLocals) {
    GlobalVariable *Control = nullptr;
    if (Emulated) {
      Control = M.getNamedGlobal(("__emutls_v." + GV->getName()).str());
      if (!Control)
        continue;
    }

    for (User *U : make_early_inc_range(GV->users())) {
      auto *II = dyn_cast<IntrinsicInst>(U);
      if (!II || II->getIntrinsicID() != Intrinsic::threadlocal_address)
        continue;
      Value *Addr = GV;
      if (Control) {
        if (!GetAddress) {
          PointerType *PtrTy = PointerType::getUnqual(M.getContext());
          GetAddress =
              M.getOrInsertFunction("__emutls_get_address", PtrTy, PtrTy);
        }
        Addr = IRBuilder<>(II).CreateCall(GetAddress, Control);
      }
      II->replaceAllUsesWith(Addr);
      II->eraseFromParent();
      Changed = true;
    }

    // Under emulated TLS the thread-local mark stays, so AsmPrinter skips the
    // variable; the initial value lives in __emutls_t.* instead.
    if (!Control) {
      GV->setThreadLocal(false);
      Changed = true;
    }
  }
  return Changed;
}

char Z80LowerThreadLocal::ID = 0;

INITIALIZE_PASS(Z80LowerThreadLocal, DEBUG_TYPE,
                "Z80 thread-local variable lowering", false, false)

ModulePass *llvm::createZ80LowerThreadLocalPass(const TargetMachine &TM) {
  return new Z80LowerThreadLocal(TM);
}
