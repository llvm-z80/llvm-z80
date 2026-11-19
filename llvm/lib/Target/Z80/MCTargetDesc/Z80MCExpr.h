//===-- Z80MCExpr.h - Z80 specific MC expression classes --------*- C++ -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_Z80_MCEXPR_H
#define LLVM_Z80_MCEXPR_H

#include "MCTargetDesc/Z80FixupKinds.h"
#include "llvm/MC/MCExpr.h"

namespace llvm {

/// An expression in Z80 machine code.
class Z80MCExpr : public MCTargetExpr {
public:
  /// Specifies the type of an expression.
  enum VariantKind {
    VK_NONE,

    VK_ADDR16_LO = MCSymbolRefExpr::FirstTargetSpecifier,
    VK_ADDR16_HI,
  };

  /// Creates a Z80 machine code expression.
  static const Z80MCExpr *create(VariantKind Kind, const MCExpr *Expr,
                                 MCContext &Ctx);

  /// Gets the type of the expression.
  VariantKind getKind() const { return Kind; }
  /// Gets the fixup which corresponds to the expression.
  Z80::Fixups getFixupKind() const;
  const MCExpr *getSubExpr() const { return SubExpr; }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;
  bool evaluateAsRelocatableImpl(MCValue &Res,
                                 const MCAssembler *Asm) const override;

  void visitUsedExpr(MCStreamer &Streamer) const override;

  MCFragment *findAssociatedFragment() const override {
    return getSubExpr()->findAssociatedFragment();
  }

  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }

private:
  int64_t evaluateAsInt64(int64_t Value) const;

  const VariantKind Kind;
  const MCExpr *SubExpr;

  explicit Z80MCExpr(VariantKind Kind, const MCExpr *Expr)
      : Kind(Kind), SubExpr(Expr) {}
};

} // end namespace llvm

#endif // LLVM_Z80_MCEXPR_H
