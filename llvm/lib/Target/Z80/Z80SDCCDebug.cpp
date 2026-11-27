//===-- Z80SDCCDebug.cpp - Z80 SDCC Debug Support -------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80SDCCDebug.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// Appends `Field` to the debug symbol `Name`.
//
// `$` separates the fields of a debug symbol, and sdas accepts only letters,
// digits, `.`, `_` and `$` in a symbol. Every other byte becomes `_`, which
// is what SDCC writes for a space or a dash. A module name keeps no dots,
// because SDCC joins a module and a function with a dot.
static void appendField(SmallVectorImpl<char> &Name, StringRef Field,
                        bool KeepDots = true) {
  for (char C : Field) {
    bool Kept = isAlnum(C) || C == '_' || (C == '.' && KeepDots);
    Name.push_back(Kept ? C : '_');
  }
}

void Z80SDCCDebug::beginFunction(const MachineFunction *MF) {
  PrevFile = nullptr;
  PrevLine = 0;
  PrevInlinedAt = nullptr;
  Subprogram = MF->getFunction().getSubprogram();

  writeFunctionSymbol(MF->getFunction());
}

void Z80SDCCDebug::beginInstruction(const MachineInstr *MI) {
  // A meta instruction prints no code. The line of a DBG_VALUE is the line
  // that declares its variable.
  if (MI->isMetaInstruction())
    return;

  // Line 0 marks code that the compiler made without a source line. Code
  // without a line keeps the latest line. In front of the first line of a
  // function, it takes the line that opens the function, as in the line
  // table of DWARF.
  const DebugLoc &DL = MI->getDebugLoc();
  if (DL && DL.getLine() != 0)
    beginLine(DL);
  else if (PrevLine == 0)
    beginScopeLine();
}

void Z80SDCCDebug::beginLine(const DebugLoc &DL) {
  // Another column of the same line is still the same line, but another call
  // site starts a new line.
  if (DL->getFile() == PrevFile && DL.getLine() == PrevLine &&
      DL.getInlinedAt() == PrevInlinedAt)
    return;

  // The CDB format has no record for inlined code. Each line that calls this
  // code gets a symbol at the same address, so a debugger can stop on a line
  // whose only code is an inlined call. A call site that the latest line was
  // already in has its symbol.
  SmallVector<const DILocation *, 4> CallSites;
  for (const DILocation *Site = DL.getInlinedAt(); Site;
       Site = Site->getInlinedAt())
    if (Site->getLine() != 0 &&
        !isOnPrevLine(Site->getFile(), Site->getLine(), Site->getInlinedAt()))
      CallSites.push_back(Site);

  // The outermost caller comes first, as in a call stack.
  for (const DILocation *Site : reverse(CallSites))
    writeLineSymbol(Site->getFile(), Site->getLine());

  // DebugLoc::print writes `file:line:col`, and then the call sites of
  // inlined code after `@[`.
  std::string Location;
  raw_string_ostream OS(Location);
  DL.print(OS);

  writeLine(DL->getFile(), DL.getLine(), DL.getInlinedAt(), Location);
}

void Z80SDCCDebug::beginScopeLine() {
  // A function without debug information or without a scope line has no
  // line to give.
  if (!Subprogram || Subprogram->getScopeLine() == 0)
    return;

  // DebugLoc::print leaves out a column of 0, so this text has no column.
  std::string Location;
  raw_string_ostream OS(Location);
  OS << Subprogram->getFilename() << ':' << Subprogram->getScopeLine();

  // A function's own opening line is never inlined code.
  writeLine(Subprogram->getFile(), Subprogram->getScopeLine(), nullptr,
            Location);
}

bool Z80SDCCDebug::isOnPrevLine(const DIFile *File, unsigned Line,
                                const DILocation *InlinedAt) const {
  if (File == PrevFile && Line == PrevLine && InlinedAt == PrevInlinedAt)
    return true;

  for (const DILocation *Site = PrevInlinedAt; Site;
       Site = Site->getInlinedAt())
    if (Site->getFile() == File && Site->getLine() == Line &&
        Site->getInlinedAt() == InlinedAt)
      return true;

  return false;
}

void Z80SDCCDebug::writeLine(const DIFile *File, unsigned Line,
                             const DILocation *InlinedAt, StringRef Text) {
  PrevFile = File;
  PrevLine = Line;
  PrevInlinedAt = InlinedAt;

  writeLineSymbol(File, Line);

  // SDCC writes the path and the line in a comment under the symbol, because
  // the symbol keeps only the name of the file.
  if (Asm.isVerbose())
    Asm.OutStreamer->emitRawComment(" " + Text);
}

void Z80SDCCDebug::writeLineSymbol(const DIFile *File, unsigned Line) {
  // SDCC names the file without its folders.
  SmallString<64> FileAndLine;
  appendField(FileAndLine, sys::path::filename(File->getFilename()));
  FileAndLine += '$';
  FileAndLine += utostr(Line);

  // The level field holds the nesting depth of the block that the line is
  // in. The handler writes no symbols of local variables, which are what a
  // debugger needs the depth for, so the depth is always `0_0`.
  unsigned Count = LineSymbolCounts[FileAndLine]++;
  writeSymbol(("C$" + FileAndLine + "$0_0$" + Twine(Count)).str());
}

void Z80SDCCDebug::writeFunctionSymbol(const Function &F) {
  // Functions with local linkage can share a name across modules, so SDCC
  // puts the module in their symbol. It names a module after its source
  // file.
  SmallString<64> Name;
  if (F.hasLocalLinkage()) {
    Name += 'F';
    appendField(Name, sys::path::stem(F.getParent()->getSourceFileName()),
                /*KeepDots=*/false);
  } else {
    Name += 'G';
  }

  Name += '$';
  appendField(Name, F.getName());
  Name += "$0$0";
  writeSymbol(Name);
}

void Z80SDCCDebug::writeSymbol(StringRef Name) {
  Asm.OutStreamer->emitRawText("\t" + Name + "\t= .");
  Asm.OutStreamer->emitRawText("\t.globl\t" + Name);
}
