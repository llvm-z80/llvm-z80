#ifndef LLVM_LIB_TARGET_Z80_Z80POSTRACOMPAREMERGE_H
#define LLVM_LIB_TARGET_Z80_Z80POSTRACOMPAREMERGE_H

#include "llvm/Pass.h"

namespace llvm {
class MachineFunctionPass;
void initializeZ80PostRACompareMergePass(PassRegistry &);
MachineFunctionPass *createZ80PostRACompareMerge();
} // namespace llvm

#endif
