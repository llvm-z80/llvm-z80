#ifndef LLVM_LIB_TARGET_Z80_TARGETINFO_Z80TARGETINFO_H
#define LLVM_LIB_TARGET_Z80_TARGETINFO_Z80TARGETINFO_H

namespace llvm {

class Target;

Target &getTheZ80Target();
Target &getTheSM83Target();

} // namespace llvm

#endif // not LLVM_LIB_TARGET_Z80_TARGETINFO_Z80TARGETINFO_H
