//===-- Z80MCExpr.cpp - Z80 specific MC expression classes ----------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80MCExpr.h"
#include "Z80FixupKinds.h"

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCValue.h"

namespace llvm {

namespace {

const struct ModifierEntry {
  const char *const Spelling;
  Z80MCExpr::VariantKind VariantKind;
  bool ImmediateOnly = false;
} ModifierNames[] = {
    // Define immediate variants of z80_8() and z80_16() first.
    {"z80_8", Z80MCExpr::VK_IMM8, true},
    {"z80_16", Z80MCExpr::VK_IMM16, true},
    {"z80_8", Z80MCExpr::VK_ADDR8},
    {"z80_16", Z80MCExpr::VK_ADDR16},
    {"z80_16lo", Z80MCExpr::VK_ADDR16_LO},
    {"z80_16hi", Z80MCExpr::VK_ADDR16_HI},
    {"z80_24", Z80MCExpr::VK_ADDR24},
    {"z80_24bank", Z80MCExpr::VK_ADDR24_BANK},
    {"z80_24segment", Z80MCExpr::VK_ADDR24_SEGMENT},
    {"z80_24segmentlo", Z80MCExpr::VK_ADDR24_SEGMENT_LO},
    {"z80_24segmenthi", Z80MCExpr::VK_ADDR24_SEGMENT_HI},
    {"z80_13", Z80MCExpr::VK_ADDR13},
};

} // end of anonymous namespace

const Z80MCExpr *Z80MCExpr::create(VariantKind Kind, const MCExpr *Expr,
                                   bool Negated, MCContext &Ctx) {
  return new (Ctx) Z80MCExpr(Kind, Expr, Negated);
}

void Z80MCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  assert(Kind != VK_NONE);

  if (isNegated()) {
    OS << '-';
  }

  OS << getName() << '(';
  MAI->printExpr(OS, *getSubExpr());
  OS << ')';
}

bool Z80MCExpr::evaluateAsConstant(int64_t &Result) const {
  MCValue Value;

  bool IsRelocatable = getSubExpr()->evaluateAsRelocatable(Value, nullptr);

  if (!IsRelocatable) {
    return false;
  }

  if (Value.isAbsolute()) {
    Result = evaluateAsInt64(Value.getConstant());
    return true;
  }

  return false;
}

bool Z80MCExpr::evaluateAsRelocatableImpl(MCValue &Result,
                                          const MCAssembler *Asm) const {
  return SubExpr->evaluateAsRelocatable(Result, Asm);
}

int64_t Z80MCExpr::evaluateAsInt64(int64_t Value) const {
  if (Negated) {
    Value *= -1;
  }

  switch (Kind) {
  case Z80MCExpr::VK_IMM8:
  case Z80MCExpr::VK_ADDR8:
  case Z80MCExpr::VK_ADDR16_LO:
  case Z80MCExpr::VK_ADDR24_SEGMENT_LO:
    Value &= 0xff;
    break;
  case Z80MCExpr::VK_ADDR16_HI:
  case Z80MCExpr::VK_ADDR24_SEGMENT_HI:
    Value &= 0xff00;
    Value >>= 8;
    break;
  case Z80MCExpr::VK_ADDR24_BANK:
    Value &= 0xff0000;
    Value >>= 16;
    break;
  case Z80MCExpr::VK_IMM16:
  case Z80MCExpr::VK_ADDR16:
  case Z80MCExpr::VK_ADDR24_SEGMENT:
    Value &= 0xffff;
    break;
  case Z80MCExpr::VK_ADDR24:
    Value &= 0xffffff;
    break;
  case Z80MCExpr::VK_ADDR13:
    Value &= 0x1fff;
    break;

  case Z80MCExpr::VK_ADDR_ASCIZ:
    llvm_unreachable("Unable to evaluate VK_ADDR_ASCIZ as int64.");

  case Z80MCExpr::VK_NONE:
    llvm_unreachable("Uninitialized expression.");
  }
  return static_cast<uint64_t>(Value);
}

Z80::Fixups Z80MCExpr::getFixupKind() const {
  Z80::Fixups Kind = Z80::Fixups::LastTargetFixupKind;

  switch (getKind()) {
  case VK_IMM8:
    Kind = Z80::Imm8;
    break;
  case VK_IMM16:
    Kind = Z80::Imm16;
    break;
  case VK_ADDR8:
    Kind = Z80::Addr8;
    break;
  case VK_ADDR16:
    Kind = Z80::Addr16;
    break;
  case VK_ADDR16_HI:
    Kind = Z80::Addr16_High;
    break;
  case VK_ADDR16_LO:
    Kind = Z80::Addr16_Low;
    break;
  case VK_ADDR24:
    Kind = Z80::Addr24;
    break;
  case VK_ADDR24_BANK:
    Kind = Z80::Addr24_Bank;
    break;
  case VK_ADDR24_SEGMENT:
    Kind = Z80::Addr24_Segment;
    break;
  case VK_ADDR24_SEGMENT_HI:
    Kind = Z80::Addr24_Segment_High;
    break;
  case VK_ADDR24_SEGMENT_LO:
    Kind = Z80::Addr24_Segment_Low;
    break;
  case VK_ADDR13:
    Kind = Z80::Addr13;
    break;
  case VK_ADDR_ASCIZ:
    Kind = Z80::AddrAsciz;
    break;
  case VK_NONE:
    llvm_unreachable("Uninitialized expression");
  }

  return Kind;
}

void Z80MCExpr::visitUsedExpr(MCStreamer &Streamer) const {
  Streamer.visitUsedExpr(*getSubExpr());
}

const char *Z80MCExpr::getName() const {
  const auto &Modifier = std::find_if(
      std::begin(ModifierNames), std::end(ModifierNames),
      [this](ModifierEntry const &Mod) { return Mod.VariantKind == Kind; });

  if (Modifier != std::end(ModifierNames)) {
    return Modifier->Spelling;
  }
  return nullptr;
}

Z80MCExpr::VariantKind Z80MCExpr::getKindByName(StringRef Name,
                                                bool IsImmediate) {
  const auto &Modifier =
      std::find_if(std::begin(ModifierNames), std::end(ModifierNames),
                   [&Name, IsImmediate](ModifierEntry const &Mod) {
                     if (Mod.ImmediateOnly && !IsImmediate)
                       return false;
                     return Mod.Spelling == Name;
                   });

  if (Modifier != std::end(ModifierNames)) {
    return Modifier->VariantKind;
  }
  return VK_NONE;
}

} // end of namespace llvm
