//===--- Z80.h - Z80 Tool and ToolChain Implementations ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_Z80_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_Z80_H

#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {
namespace z80 {

/// SDCC external assembler (sdasz80 / sdasgb).
/// Used when -fno-integrated-as is specified.
class LLVM_LIBRARY_VISIBILITY Assembler final : public Tool {
public:
  Assembler(const ToolChain &TC) : Tool("z80::Assembler", "sdasz80", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

/// SDCC linker (sdldz80 / sdldgb).
/// Used when -fno-integrated-as is specified (SDCC .rel object format).
class LLVM_LIBRARY_VISIBILITY SDCCLinker final : public Tool {
public:
  SDCCLinker(const ToolChain &TC)
      : Tool("z80::SDCCLinker", "sdldz80", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

/// ELF linker (ld.lld).
/// Default linker when using the integrated assembler.
class LLVM_LIBRARY_VISIBILITY Linker final : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("z80::Linker", "ld.lld", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace z80
} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY Z80ToolChain : public ToolChain {
public:
  Z80ToolChain(const Driver &D, const llvm::Triple &Triple,
               const llvm::opt::ArgList &Args);

protected:
  Tool *buildAssembler() const override;
  Tool *buildLinker() const override;

public:
  bool IsIntegratedAssemblerDefault() const override {
    return getTriple().getEnvironment() != llvm::Triple::SDCC;
  }
  bool isPICDefault() const override { return false; }
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool isPICDefaultForced() const override { return false; }
  bool HasNativeLLVMSupport() const override { return true; }

  void
  addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                        llvm::opt::ArgStringList &CC1Args,
                        Action::OffloadKind DeviceOffloadKind) const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_Z80_H
