//===-- Z80FixupKinds.h - Z80 Specific Fixup Entries ------------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_FIXUP_KINDS_H
#define LLVM_Z80_FIXUP_KINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace Z80 {

/// The set of supported fixups.
///
/// Although most of the current fixup types reflect a unique relocation
/// one can have multiple fixup types for a given relocation and thus need
/// to be uniquely named.
///
/// \note This table *must* be in the same order of
///       MCFixupKindInfo Infos[] in Z80AsmBackend::getFixupKindInfo().
enum Fixups {
  Imm8 = FirstTargetFixupKind, // An 8-bit immediate value.
  Imm16,                       // A 16-bit immediate value.
  Addr8,                       // An 8-bit I/O port number.
  Addr16,                      // A 16-bit address.
  Addr16_Low,                  // The low byte of a 16-bit address.
  Addr16_High,                 // The high byte of a 16-bit address.
  PCRel8,                      // An 8-bit PC relative value.
  Disp8,                       // A signed 8-bit (ix+d) or SP offset.
  LDH8,                        // An 8-bit offset from 0xFF00 for LDH.
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace Z80
} // namespace llvm

#endif // LLVM_Z80_FIXUP_KINDS_H
