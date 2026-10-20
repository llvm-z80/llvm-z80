//===- Loader.h - Load a program image for z80-run --------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef Z80_TEST_TOOLS_Z80_RUN_LOADER_H
#define Z80_TEST_TOOLS_Z80_RUN_LOADER_H

#include "TicksGlue.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Error.h"
#include <optional>
#include <vector>

namespace llvm {
namespace z80run {

class LoadError : public ErrorInfo<LoadError> {
public:
  static char ID;

  LoadError(StringRef Kind, const Twine &Message)
      : Kind(Kind.str()), Message(Message.str()) {}

  StringRef kind() const { return Kind; }
  void log(raw_ostream &OS) const override { OS << Message; }
  std::error_code convertToErrorCode() const override {
    return inconvertibleErrorCode();
  }

private:
  std::string Kind;
  std::string Message;
};

struct Program {
  std::vector<uint8_t> Image = std::vector<uint8_t>(0x10000);
  std::optional<Z80RunCPU> CPU;
  uint16_t Entry = 0;
  StringMap<uint16_t> Symbols;
};

Expected<Program> loadProgram(StringRef Path);

} // namespace z80run
} // namespace llvm

#endif
