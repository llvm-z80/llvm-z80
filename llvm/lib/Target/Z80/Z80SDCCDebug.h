//===-- Z80SDCCDebug.h - Z80 SDCC Debug Support ---------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_Z80_Z80SDCCDEBUG_H
#define LLVM_LIB_TARGET_Z80_Z80SDCCDEBUG_H

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/AsmPrinterHandler.h"

namespace llvm {

class DIFile;
class DILocation;
class DISubprogram;
class DebugLoc;
class Function;

/// Writes the debug symbols of SDCC into an sdasz80 listing.
///
/// AsmPrinter adds DwarfDebug only to the ELF format, so an sdasz80 listing
/// gets no `.loc` lines. SDCC marks code with global symbols instead, and
/// sdld copies them into the CDB file that a debugger reads. This handler
/// writes the symbols that mark source lines and functions.
///
///   C$<file>$<line>$<level>$<block>  The code of a source line starts here.
///   G$<name>$0$0                     A function starts here.
///   F<module>$<name>$0$0             A function with local linkage starts
///                                    here.
///
/// AsmPrinter calls the handler before it prints each instruction. An
/// instruction that starts a new source line gets a line symbol, and one more
/// for each line that calls it as inlined code.
class Z80SDCCDebug : public AsmPrinterHandler {
  /// Holds the printer whose streamer receives the symbols.
  AsmPrinter &Asm;

  /// Hold the file, line and call site of the latest line in the current
  /// function. The call site is null for code that is not inlined.
  ///
  /// LLVM keeps one node per distinct DIFile and DILocation, so equal
  /// pointers mean the same file and the same call.
  const DIFile *PrevFile = nullptr;
  unsigned PrevLine = 0;
  const DILocation *PrevInlinedAt = nullptr;

  /// Holds the debug record of the current function, or null when the
  /// function has no debug information.
  const DISubprogram *Subprogram = nullptr;

  /// Counts the line symbols of each `<file>$<line>` in the module.
  ///
  /// A line can have code in several places. The block field of its symbol
  /// takes this count, so each place gets a symbol of its own.
  StringMap<unsigned> LineSymbolCounts;

  /// Writes the symbols of `DL` when it starts a new line.
  void beginLine(const DebugLoc &DL);

  /// Writes the symbol of the line that opens the current function.
  void beginScopeLine();

  /// Returns whether the latest line or one of its call sites is `Line` of
  /// `File`, inlined at `InlinedAt`.
  bool isOnPrevLine(const DIFile *File, unsigned Line,
                    const DILocation *InlinedAt) const;

  /// Remembers the latest line and writes its symbol. A verbose listing
  /// also gets `Text` as a comment.
  void writeLine(const DIFile *File, unsigned Line, const DILocation *InlinedAt,
                 StringRef Text);

  /// Writes the symbol that marks the start of `Line` of `File`.
  void writeLineSymbol(const DIFile *File, unsigned Line);

  /// Writes the symbol that marks the start of `F`.
  void writeFunctionSymbol(const Function &F);

  /// Defines the global symbol `Name` at the current address.
  void writeSymbol(StringRef Name);

public:
  explicit Z80SDCCDebug(AsmPrinter &Asm) : Asm(Asm) {}

  /// The handler writes nothing at the end of a module.
  void endModule() override {}

  /// Forgets the latest line, so the first line of the function gets a
  /// symbol, and writes the symbol of the function.
  void beginFunction(const MachineFunction *MF) override;

  /// The handler writes nothing at the end of a function.
  void endFunction(const MachineFunction *) override {}

  /// Writes the line symbols of `MI` when it starts a new source line.
  void beginInstruction(const MachineInstr *MI) override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_Z80_Z80SDCCDEBUG_H
