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
  case R_Z80_PCREL_8:
  case R_Z80_PCREL_16:
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
  case R_Z80_IMM8:
  case R_Z80_ADDR8:
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_Z80_ADDR16:
  case R_Z80_IMM16:
  case R_Z80_ADDR24_SEGMENT:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_Z80_ADDR16_LO:
  case R_Z80_ADDR24_SEGMENT_LO:
    *loc = val;
    break;
  case R_Z80_ADDR16_HI:
  case R_Z80_ADDR24_SEGMENT_HI:
    *loc = val >> 8;
    break;
  case R_Z80_PCREL_8: {
    // Z80 JR displacement: target = PC_after_instr + d = (loc + 1) + d
    // lld computes val = S + A - P where P = loc, so d = val - 1
    int64_t offset = (int64_t)val - 1;
    checkInt(ctx, loc, offset, 8, rel);
    *loc = offset;
    break;
  }
  case R_Z80_PCREL_16:
    checkInt(ctx, loc, (int64_t)val - 1, 16, rel);
    write16le(loc, val - 1);
    break;
  case R_Z80_ADDR24: {
    checkIntUInt(ctx, loc, val, 24, rel);
    uint32_t existing = read32le(loc);
    write32le(loc, (existing & 0xFF000000) | (val & 0x00FFFFFF));
    break;
  }
  case R_Z80_ADDR24_BANK:
    *loc = val >> 16;
    break;
  case R_Z80_ADDR13:
    checkUInt(ctx, loc, val, 13, rel);
    write16le(loc, val);
    break;
  case R_Z80_ADDR_ASCIZ:
    // Decimal ASCII address encoding — treated as 16-bit absolute address.
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_Z80_DISP8: {
    // 8-bit signed displacement for indexed addressing (IX+d, IY+d).
    int64_t sval = (int64_t)val;
    checkInt(ctx, loc, sval, 8, rel);
    *loc = sval;
    break;
  }
  case R_SM83_LDH8: {
    // SM83 LDH instruction: operand is (address - 0xFF00).
    int64_t offset = (int64_t)val - 0xFF00;
    checkUInt(ctx, loc, offset, 8, rel);
    *loc = offset;
    break;
  }
  case R_Z80_FK_DATA_4:
    write32le(loc, val);
    break;
  case R_Z80_FK_DATA_8:
    write64le(loc, val);
    break;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setZ80TargetInfo(Ctx &ctx) { ctx.target.reset(new Z80(ctx)); }
