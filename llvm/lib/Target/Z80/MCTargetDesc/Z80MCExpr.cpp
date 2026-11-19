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
} ModifierNames[] = {
    {"z80_16lo", Z80MCExpr::VK_ADDR16_LO},
    {"z80_16hi", Z80MCExpr::VK_ADDR16_HI},
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

  // Print z80_16lo and z80_16hi as arithmetic for z80asm.
  if (MAI->isZ88DK()) {
    OS << '(';
    MAI->printExpr(OS, *getSubExpr());
    OS << (Kind == VK_ADDR16_LO ? ") & 0xff" : ") >> 8");
    return;
  }

  // Print z80_16lo(x) as <(x) and z80_16hi(x) as >(x) for sdasz80.
  if (MAI->isSDCC()) {
    OS << (Kind == VK_ADDR16_LO ? "<(" : ">(");
    MAI->printExpr(OS, *getSubExpr());
    OS << ')';
    return;
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
  if (!SubExpr->evaluateAsRelocatable(Result, Asm))
    return false;
  // Fold the byte of a constant. Otherwise the relocation selects the byte.
  if (Result.isAbsolute())
    Result = MCValue::get(evaluateAsInt64(Result.getConstant()));
  else
    Result.setSpecifier(Kind);
  return true;
}

int64_t Z80MCExpr::evaluateAsInt64(int64_t Value) const {
  if (Negated) {
    Value *= -1;
  }

  switch (Kind) {
  case Z80MCExpr::VK_ADDR16_LO:
    Value &= 0xff;
    break;
  case Z80MCExpr::VK_ADDR16_HI:
    Value &= 0xff00;
    Value >>= 8;
    break;
  case Z80MCExpr::VK_NONE:
    llvm_unreachable("Uninitialized expression.");
  }
  return static_cast<uint64_t>(Value);
}

Z80::Fixups Z80MCExpr::getFixupKind() const {
  Z80::Fixups Kind = Z80::Fixups::LastTargetFixupKind;

  switch (getKind()) {
  case VK_ADDR16_HI:
    Kind = Z80::Addr16_High;
    break;
  case VK_ADDR16_LO:
    Kind = Z80::Addr16_Low;
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

Z80MCExpr::VariantKind Z80MCExpr::getKindByName(StringRef Name) {
  const auto &Modifier =
      std::find_if(std::begin(ModifierNames), std::end(ModifierNames),
                   [&Name](ModifierEntry const &Mod) {
                     return Name.equals_insensitive(Mod.Spelling);
                   });

  if (Modifier != std::end(ModifierNames)) {
    return Modifier->VariantKind;
  }
  return VK_NONE;
}

} // end of namespace llvm
