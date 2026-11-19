//===-- Z80MCExpr.cpp - Z80 specific MC expression classes ----------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Z80MCExpr.h"

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCValue.h"

namespace llvm {

const Z80MCExpr *Z80MCExpr::create(VariantKind Kind, const MCExpr *Expr,
                                   MCContext &Ctx) {
  return new (Ctx) Z80MCExpr(Kind, Expr);
}

void Z80MCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  assert(Kind != VK_NONE);

  // sdasz80 selects bytes with <(x) and >(x).
  if (MAI->isSDCC()) {
    OS << (Kind == VK_ADDR16_LO ? "<(" : ">(");
    MAI->printExpr(OS, *getSubExpr());
    OS << ')';
    return;
  }

  // GNU as and z80asm select bytes with (x)&0xff and (x)>>8.
  OS << '(';
  MAI->printExpr(OS, *getSubExpr());
  OS << (Kind == VK_ADDR16_LO ? ")&0xff" : ")>>8");
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
  return Value;
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

} // end of namespace llvm
