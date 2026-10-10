//===- Z80.cpp ------------------------------------------------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The Z80 is an 8-bit microprocessor with a 16-bit address bus (64KB address
// space). This is a static-only target with no dynamic linking, PLT/GOT,
// or TLS support. The Z80 backend also covers the SM83 (Game Boy CPU).
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class Z80 final : public TargetInfo {
public:
  Z80(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

Z80::Z80(Ctx &ctx) : TargetInfo(ctx) {
  // HALT instruction (0x76) as trap/padding.
  trapInstr = {0x76, 0x76, 0x76, 0x76};
  defaultImageBase = 0;
  defaultMaxPageSize = 1;
  defaultCommonPageSize = 1;
}

RelExpr Z80::getRelExpr(RelType type, const Symbol &s,
                        const uint8_t *loc) const {
  switch (type) {
  case R_Z80_8_PCREL:
    return R_PC;
  case R_Z80_NONE:
    return R_NONE;
  default:
    return R_ABS;
  }
}

void Z80::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_Z80_NONE:
    break;
  case R_Z80_8: {
    // Same range check as binutils, which also accepts an LDH high-page
    // address.
    uint8_t hi = (val >> 8) & 0xff;
    if (hi != 0 && hi != 0xff)
      reportRangeError(ctx, loc, rel, Twine((int64_t)val), -256, 255);
    *loc = val;
    break;
  }
  case R_Z80_8_DIS:
  case R_Z80_8_PCREL:
    checkInt(ctx, loc, (int64_t)val, 8, rel);
    *loc = val;
    break;
  case R_Z80_16:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_Z80_16_BE:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  case R_Z80_24:
    checkIntUInt(ctx, loc, val, 24, rel);
    loc[0] = val;
    loc[1] = val >> 8;
    loc[2] = val >> 16;
    break;
  case R_Z80_32:
    write32le(loc, val);
    break;
  case R_Z80_BYTE0:
    *loc = val;
    break;
  case R_Z80_BYTE1:
    *loc = val >> 8;
    break;
  case R_Z80_BYTE2:
    *loc = val >> 16;
    break;
  case R_Z80_BYTE3:
    *loc = val >> 24;
    break;
  case R_Z80_WORD0:
    write16le(loc, val);
    break;
  case R_Z80_WORD1:
    write16le(loc, val >> 16);
    break;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setZ80TargetInfo(Ctx &ctx) { ctx.target.reset(new Z80(ctx)); }
