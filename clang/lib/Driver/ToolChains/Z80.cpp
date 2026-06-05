//===--- Z80.cpp - Z80 ToolChain Implementations ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

//===----------------------------------------------------------------------===//
// SDCC Assembler (sdasz80 / sdasgb) — used with -fno-integrated-as
//===----------------------------------------------------------------------===//

void z80::Assembler::ConstructJob(Compilation &C, const JobAction &JA,
                                  const InputInfo &Output,
                                  const InputInfoList &Inputs,
                                  const ArgList &Args,
                                  const char *LinkingOutput) const {
  claimNoWarnArgs(Args);
  ArgStringList CmdArgs;

  // -g: make undefined symbols global (needed for external references)
  CmdArgs.push_back("-g");
  // -o: use outfile mode to control output path directly.
  // sdasz80 outfile mode: sdasz80 [options] -o outfile input1 [input2 ...]
  // With .rel extension on outfile, sdasz80 writes to that exact path.
  CmdArgs.push_back("-o");

  // sdasz80 requires .rel extension on the output file. Assemble to a .rel
  // path, then rename to the driver-expected .o path so the rest of the
  // pipeline (linker, -save-temps, -c) sees the correct filename.
  SmallString<256> RelOut(Output.getFilename());
  llvm::sys::path::replace_extension(RelOut, ".rel");
  CmdArgs.push_back(Args.MakeArgString(RelOut));

  for (const auto &II : Inputs)
    CmdArgs.push_back(II.getFilename());

  const char *AsmName =
      getToolChain().getTriple().isSM83() ? "sdasgb" : "sdasz80";
  const char *Exec = Args.MakeArgString(getToolChain().GetProgramPath(AsmName));
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Exec, CmdArgs, Inputs, Output));

  // Rename .rel to the .o path the driver expects.
  if (StringRef(Output.getFilename()) != StringRef(RelOut)) {
    ArgStringList MvArgs;
    MvArgs.push_back(Args.MakeArgString(RelOut));
    MvArgs.push_back(Output.getFilename());
    const char *MvExec =
        Args.MakeArgString(getToolChain().GetProgramPath("mv"));
    C.addCommand(std::make_unique<Command>(JA, *this,
                                           ResponseFileSupport::None(), MvExec,
                                           MvArgs, Inputs, Output));
  }
}

//===----------------------------------------------------------------------===//
// SDCC Linker (sdldz80 / sdldgb) — used with -fno-integrated-as
//===----------------------------------------------------------------------===//

void z80::SDCCLinker::ConstructJob(Compilation &C, const JobAction &JA,
                                   const InputInfo &Output,
                                   const InputInfoList &Inputs,
                                   const ArgList &Args,
                                   const char *LinkingOutput) const {
  const auto &TC = static_cast<const Z80ToolChain &>(getToolChain());

  ArgStringList CmdArgs;

  // -i: produce Intel HEX output (.ihx)
  CmdArgs.push_back("-i");

  // sdldz80 convention: first non-option argument is the output base name.
  // sdldz80 strips any extension and appends .ihx (e.g. "a.out" -> "a.ihx").
  // We pre-strip the extension so our IhxFile computation matches.
  SmallString<256> OutputBase(Output.getFilename());
  llvm::sys::path::replace_extension(OutputBase, "");
  CmdArgs.push_back(Args.MakeArgString(OutputBase));

  // Select target-specific tools and runtime paths.
  bool IsSM83 = TC.getTriple().isSM83();
  const char *SubDir = IsSM83 ? "sm83" : "z80";
  const char *RtLibName = IsSM83 ? "sm83_rt.lib" : "z80_rt.lib";
  const char *RtLibBase = IsSM83 ? "sm83_rt" : "z80_rt";
  const char *LinkerName = IsSM83 ? "sdldgb" : "sdldz80";

  // SDCC toolchain: no custom crt0 — SDCC's own startup handles initialization.

  // Add input object files.
  for (const auto &II : Inputs) {
    if (II.isFilename())
      CmdArgs.push_back(II.getFilename());
  }

  // Auto-link runtime library archive if found.
  // Use -k <dir> -l <name> for selective linking (only referenced modules).
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    SmallString<256> LibDir(TC.getDriver().Dir);
    llvm::sys::path::append(LibDir, "..", "lib", SubDir);
    SmallString<256> LibFile(LibDir);
    llvm::sys::path::append(LibFile, RtLibName);
    if (llvm::sys::fs::exists(LibFile)) {
      CmdArgs.push_back("-k");
      CmdArgs.push_back(Args.MakeArgString(LibDir));
      CmdArgs.push_back("-l");
      CmdArgs.push_back(RtLibBase);
    }
  }

  // Forward user linker flags (-Wl, -Xlinker).
  Args.AddAllArgs(CmdArgs, options::OPT_Wl_COMMA);
  Args.AddAllArgs(CmdArgs, options::OPT_Xlinker);

  const char *Exec = Args.MakeArgString(TC.GetProgramPath(LinkerName));
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Exec, CmdArgs, Inputs, Output));

  // sdldz80 produces <output_base>.ihx.
  // Only rename if the user explicitly specified -o with a non-.ihx name.
  // Otherwise, let the .ihx file be the final output (e.g. default: a.ihx).
  SmallString<256> IhxFile(OutputBase);
  IhxFile += ".ihx";
  if (Args.hasArg(options::OPT_o) &&
      StringRef(Output.getFilename()) != StringRef(IhxFile)) {
    ArgStringList MvArgs;
    MvArgs.push_back(Args.MakeArgString(IhxFile));
    MvArgs.push_back(Output.getFilename());

    const char *MvExec = Args.MakeArgString(TC.GetProgramPath("mv"));
    C.addCommand(std::make_unique<Command>(JA, *this,
                                           ResponseFileSupport::None(), MvExec,
                                           MvArgs, Inputs, Output));
  }
}

//===----------------------------------------------------------------------===//
// ELF Linker (ld.lld) — default when using integrated assembler
//===----------------------------------------------------------------------===//

void z80::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                               const InputInfo &Output,
                               const InputInfoList &Inputs, const ArgList &Args,
                               const char *LinkingOutput) const {
  const auto &TC = static_cast<const Z80ToolChain &>(getToolChain());

  ArgStringList CmdArgs;

  bool IsSM83 = TC.getTriple().isSM83();
  const char *SubDir = IsSM83 ? "sm83" : "z80";

  // No dynamic linking on Z80
  CmdArgs.push_back("-static");

  // Use the default linker script (flat memory, .text at 0x0000).
  const char *LDScriptName = IsSM83 ? "sm83.ld" : "z80.ld";
  SmallString<256> LDScript(TC.getDriver().Dir);
  llvm::sys::path::append(LDScript, "..", "lib", SubDir, LDScriptName);
  if (llvm::sys::fs::exists(LDScript)) {
    CmdArgs.push_back("-T");
    CmdArgs.push_back(Args.MakeArgString(LDScript));
  } else {
    // Fallback: set entry point manually if no linker script found.
    CmdArgs.push_back("-e");
    CmdArgs.push_back("_start");
  }

  // Output
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // Link crt0 first so startup code is at address 0x0000.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles)) {
    SmallString<256> Crt0Path(TC.getDriver().Dir);
    llvm::sys::path::append(Crt0Path, "..", "lib", SubDir,
                             IsSM83 ? "sm83_crt0.o" : "z80_crt0.o");
    if (llvm::sys::fs::exists(Crt0Path))
      CmdArgs.push_back(Args.MakeArgString(Crt0Path));
  }

  // Add input object files.
  for (const auto &II : Inputs) {
    if (II.isFilename())
      CmdArgs.push_back(II.getFilename());
  }

  // Auto-link runtime library archive if found.
  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    SmallString<256> RtLib(TC.getDriver().Dir);
    llvm::sys::path::append(RtLib, "..", "lib", SubDir,
                             IsSM83 ? "sm83_rt.a" : "z80_rt.a");
    if (llvm::sys::fs::exists(RtLib))
      CmdArgs.push_back(Args.MakeArgString(RtLib));
  }

  // Forward user linker flags (-Wl, -Xlinker).
  Args.AddAllArgs(CmdArgs, options::OPT_Wl_COMMA);
  Args.AddAllArgs(CmdArgs, options::OPT_Xlinker);

  const char *Exec = Args.MakeArgString(TC.GetProgramPath("ld.lld"));
  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Exec, CmdArgs, Inputs, Output));
}

//===----------------------------------------------------------------------===//
// Z80 ToolChain
//===----------------------------------------------------------------------===//

Z80ToolChain::Z80ToolChain(const Driver &D, const llvm::Triple &Triple,
                           const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  getProgramPaths().push_back(getDriver().Dir);
}

Tool *Z80ToolChain::buildAssembler() const {
  return new tools::z80::Assembler(*this);
}

Tool *Z80ToolChain::buildLinker() const {
  // Linker selection is based on the target triple, not the assembler choice.
  // SDCC triples (z80-*-*-sdcc) produce .rel objects for sdldz80/sdldgb;
  // ELF triples (z80, sm83) produce ELF objects for ld.lld.
  if (getTriple().getEnvironment() == llvm::Triple::SDCC)
    return new tools::z80::SDCCLinker(*this);
  return new tools::z80::Linker(*this);
}

void Z80ToolChain::addClangTargetOptions(const ArgList &DriverArgs,
                                         ArgStringList &CC1Args,
                                         Action::OffloadKind) const {
  // When using the external SDCC assembler, emit sdasz80 assembly format.
  // With the integrated assembler, assembly goes directly through MC.
  if (!useIntegratedAs()) {
    // ELF triples require the integrated assembler — sdasz80 produces .rel
    // files incompatible with ld.lld. Use -target z80-*-*-sdcc for the
    // external assembler toolchain.
    if (getTriple().getEnvironment() != llvm::Triple::SDCC) {
      getDriver().Diag(diag::err_drv_unsupported_opt_for_target)
          << "-fno-integrated-as" << getTriple().str();
    }
    CC1Args.push_back("-mllvm");
    CC1Args.push_back("-z80-asm-format=sdasz80");
  }

  // Disable features not applicable to Z80 bare metal.
  CC1Args.push_back("-fno-use-cxa-atexit");

  // Z80 has no branch predictor and only 3 register pairs.
  // Speculating computations across branches (select formation) always
  // increases register pressure without any pipeline benefit.
  // Disable PHI node folding to keep if-else as branches.
  CC1Args.push_back("-mllvm");
  CC1Args.push_back("-two-entry-phi-node-folding-threshold=0");
}
