//===-- Z80MCTargetDesc.h - Z80 Target Descriptions -------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Z80 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_MCTARGET_DESC_H
#define LLVM_Z80_MCTARGET_DESC_H

#include "llvm/ADT/Sequence.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/Support/DataTypes.h"

#include <memory>

namespace llvm {

class FeatureBitset;
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class StringRef;
class Target;
class Triple;
class raw_pwrite_stream;

Target &getTheZ80Target();
Target &getTheSM83Target();

MCInstrInfo *createZ80MCInstrInfo();

/// Creates a machine code emitter for Z80.
MCCodeEmitter *createZ80MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);

/// Creates an assembly backend for Z80.
MCAsmBackend *createZ80AsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                  const MCRegisterInfo &MRI,
                                  const llvm::MCTargetOptions &TO);

/// Creates an ELF object writer for Z80.
std::unique_ptr<MCObjectTargetWriter> createZ80ELFObjectWriter(uint8_t OSABI);

} // end namespace llvm

#define GET_REGINFO_ENUM
#include "Z80GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "Z80GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "Z80GenSubtargetInfo.inc"

namespace llvm {
template <> struct enum_iteration_traits<decltype(Z80::NoRegister)> {
  static constexpr bool is_iterable = true;
};

namespace Z80Op {

enum OperandType : unsigned {
  OPERAND_IMM8 = MCOI::OPERAND_FIRST_TARGET,
  OPERAND_ADDR8,
  OPERAND_ADDR16,
  OPERAND_IMM16,
  OPERAND_IMM3,
  OPERAND_ADDR24,
  OPERAND_IMM24,
  OPERAND_ADDR13,
  OPERAND_IMM4,
  OPERAND_PORT8,
  OPERAND_PCREL,
  OPERAND_DISP8,
};

} // namespace Z80Op

} // namespace llvm

#endif // LLVM_Z80_MCTARGET_DESC_H
