//===---- Z80AsmParser.cpp - Parse Z80 assembly to MCInst instructions ----===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Z80 assembly parser implementation.
// Supports both LLVM/ELF and sdasz80 (SDCC) assembly dialects:
//
// LLVM format:                sdasz80 format:
//   ld a, (ix+5)               ld a, 5(ix)
//   ld a, (hl)                 ld a, (hl)
//   ld (0x1234), a             ld (_label), a
//   ld a, 42                   ld a, #42
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "TargetInfo/Z80TargetInfo.h"
#include "Z80.h"
#include "Z80RegisterInfo.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"

#include <memory>

#define DEBUG_TYPE "z80-asm-parser"

using namespace llvm;

namespace {

/// Z80Operand - Represents a parsed Z80 operand.
class Z80Operand : public MCParsedAsmOperand {
public:
  enum KindTy {
    k_Token,     // Mnemonic, condition code, or punctuation token
    k_Register,  // Register operand
    k_Immediate, // Immediate value
  };

private:
  KindTy Kind;
  SMLoc Start, End;

  StringRef Tok;
  unsigned RegNum;
  const MCExpr *ImmVal;

public:
  Z80Operand(KindTy K, SMLoc S, SMLoc E)
      : Kind(K), Start(S), End(E), RegNum(0), ImmVal(nullptr) {}

  // Token operand
  static std::unique_ptr<Z80Operand> CreateToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<Z80Operand>(k_Token, S, S);
    Op->Tok = Str;
    return Op;
  }

  // Register operand
  static std::unique_ptr<Z80Operand> CreateReg(unsigned Reg, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<Z80Operand>(k_Register, S, E);
    Op->RegNum = Reg;
    return Op;
  }

  // Immediate operand
  static std::unique_ptr<Z80Operand> CreateImm(const MCExpr *Val, SMLoc S,
                                               SMLoc E) {
    auto Op = std::make_unique<Z80Operand>(k_Immediate, S, E);
    Op->ImmVal = Val;
    return Op;
  }

  bool isToken() const override { return Kind == k_Token; }
  bool isImm() const override { return Kind == k_Immediate; }
  bool isReg() const override { return Kind == k_Register; }
  bool isMem() const override { return false; }

  // Immediate predicates for different sizes
  bool isImm3() const {
    if (!isImm())
      return false;
    if (const auto *CE = dyn_cast<MCConstantExpr>(ImmVal))
      return CE->getValue() >= 0 && CE->getValue() <= 7;
    return true;
  }

  bool isImm8() const {
    if (!isImm())
      return false;
    if (const auto *CE = dyn_cast<MCConstantExpr>(ImmVal))
      return CE->getValue() >= -128 && CE->getValue() <= 255;
    return true;
  }

  bool isImm16() const {
    if (!isImm())
      return false;
    if (const auto *CE = dyn_cast<MCConstantExpr>(ImmVal))
      return CE->getValue() >= -32768 && CE->getValue() <= 65535;
    return true;
  }

  bool isPCRel8() const { return isImm(); }
  bool isAddr16() const { return isImm(); }

  bool isDisp8() const {
    if (!isImm())
      return false;
    if (const auto *CE = dyn_cast<MCConstantExpr>(ImmVal))
      return CE->getValue() >= -128 && CE->getValue() <= 127;
    return true;
  }

  bool isPort8() const {
    if (!isImm())
      return false;
    if (const auto *CE = dyn_cast<MCConstantExpr>(ImmVal))
      return CE->getValue() >= 0 && CE->getValue() <= 255;
    return true;
  }

  StringRef getToken() const {
    assert(Kind == k_Token && "Invalid access!");
    return Tok;
  }

  MCRegister getReg() const override {
    assert(Kind == k_Register && "Invalid access!");
    return RegNum;
  }

  const MCExpr *getImm() const {
    assert(Kind == k_Immediate && "Invalid access!");
    return ImmVal;
  }

  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Register && "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(RegNum));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Immediate && "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, ImmVal);
  }

  void addImm3Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }
  void addImm8Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }
  void addImm16Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }
  void addPCRel8Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }
  void addAddr16Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }
  void addDisp8Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }
  void addPort8Operands(MCInst &Inst, unsigned N) const {
    addImmOperands(Inst, N);
  }

  void addExpr(MCInst &Inst, const MCExpr *Expr) const {
    if (!Expr)
      Inst.addOperand(MCOperand::createImm(0));
    else if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case k_Token:
      OS << "Token: " << Tok;
      break;
    case k_Register:
      OS << "Reg: " << RegNum;
      break;
    case k_Immediate:
      OS << "Imm: <expr>";
      break;
    }
    OS << "\n";
  }
};

/// Z80AsmParser - The Z80 assembly parser.
class Z80AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;
  const MCRegisterInfo *MRI;

public:
  enum Z80MatchResultTy {
    Match_immediate = FIRST_TARGET_MATCH_RESULT_TY,
    Match_InvalidPCRel8,
    Match_InvalidAddr16,
  };

private:
#define GET_ASSEMBLER_HEADER
#include "Z80GenAsmMatcher.inc"

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  ParseStatus parseDirective(AsmToken DirectiveID) override;

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

  // Helper methods
  MCRegister parseRegisterName(StringRef Name);
  MCRegister tryParseRegisterName();
  bool tryParseRegisterOperand(OperandVector &Operands);
  bool parseOperand(OperandVector &Operands, StringRef Mnemonic);
  bool parseParenOperand(OperandVector &Operands);
  bool parseSDASZ80Indexed(OperandVector &Operands, const MCExpr *Disp,
                           SMLoc DispLoc);
  bool parseExpression(const MCExpr *&Expr);
  void eatComma();

  /// Get the compound token string for a register indirect like "(hl)".
  /// Returns nullptr if the register doesn't use compound tokens.
  static const char *getCompoundToken(MCRegister Reg) {
    switch (Reg) {
    case Z80::HL: return "(hl)";
    case Z80::BC: return "(bc)";
    case Z80::DE: return "(de)";
    case Z80::SP: return "(sp)";
    case Z80::C:  return "(c)";
    case Z80::IX: return "(ix)";
    case Z80::IY: return "(iy)";
    default: return nullptr;
    }
  }

  bool emit(MCInst &Inst, SMLoc const &Loc, MCStreamer &Out) const;
  bool invalidOperand(SMLoc const &Loc, OperandVector const &Operands,
                      uint64_t const &ErrorInfo);
  bool missingFeature(SMLoc const &Loc, uint64_t const &ErrorInfo);

public:
  Z80AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
               const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    MRI = getContext().getRegisterInfo();
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }

  MCAsmParser &getParser() const { return Parser; }
  AsmLexer &getLexer() const { return Parser.getLexer(); }
};

} // end anonymous namespace

// Auto-generated Match Functions from TableGen
static MCRegister MatchRegisterName(StringRef Name);
static MCRegister MatchRegisterAltName(StringRef Name);

MCRegister Z80AsmParser::parseRegisterName(StringRef Name) {
  if (Name.contains('\''))
    return MCRegister();

  MCRegister Reg = MatchRegisterName(Name);
  if (Reg)
    return Reg;

  Reg = MatchRegisterAltName(Name);
  if (Reg)
    return Reg;

  Reg = MatchRegisterName(Name.lower());
  if (Reg)
    return Reg;

  Reg = MatchRegisterAltName(Name.lower());
  if (Reg)
    return Reg;

  Reg = MatchRegisterName(Name.upper());
  if (Reg)
    return Reg;

  return MatchRegisterAltName(Name.upper());
}

MCRegister Z80AsmParser::tryParseRegisterName() {
  if (Parser.getTok().isNot(AsmToken::Identifier))
    return MCRegister();

  StringRef Name = Parser.getTok().getString();
  return parseRegisterName(Name);
}

bool Z80AsmParser::tryParseRegisterOperand(OperandVector &Operands) {
  MCRegister Reg = tryParseRegisterName();

  if (!Reg)
    return true;

  SMLoc S = Parser.getTok().getLoc();
  SMLoc E = Parser.getTok().getEndLoc();
  Operands.push_back(Z80Operand::CreateReg(Reg, S, E));
  Parser.Lex(); // Eat register token

  return false;
}

bool Z80AsmParser::parseExpression(const MCExpr *&Expr) {
  return getParser().parseExpression(Expr);
}

/// Parse a parenthesized operand: (reg), (expr), (ix+d), (iy+d).
/// Produces token sequences matching the AsmMatcher expectations:
///   (hl)     → compound token "(hl)"
///   (nn)     → tokens "(", Imm, ")"
///   (ix+d)   → tokens "(", Reg IX, "+", Imm, ")"
///   (ix-d)   → tokens "(", Reg IX, "+", Imm(-d), ")"
///   (ix)     → compound token "(ix)"
bool Z80AsmParser::parseParenOperand(OperandVector &Operands) {
  SMLoc LParenLoc = Parser.getTok().getLoc();
  Parser.Lex(); // Eat '('

  // Check for register inside parentheses
  if (Parser.getTok().is(AsmToken::Identifier)) {
    MCRegister Reg = tryParseRegisterName();
    if (Reg) {
      SMLoc RegLoc = Parser.getTok().getLoc();
      SMLoc RegEnd = Parser.getTok().getEndLoc();
      Parser.Lex(); // Eat register name

      // SM83 auto-increment/decrement: (hl+) or (hl-)
      if (Reg == Z80::HL &&
          (Parser.getTok().is(AsmToken::Plus) ||
           Parser.getTok().is(AsmToken::Minus))) {
        bool IsPlus = Parser.getTok().is(AsmToken::Plus);
        Parser.Lex(); // Eat +/-

        if (Parser.getTok().isNot(AsmToken::RParen))
          return Error(Parser.getTok().getLoc(), "expected ')'");
        Parser.Lex(); // Eat ')'

        Operands.push_back(Z80Operand::CreateToken(
            IsPlus ? "(hl+)" : "(hl-)", LParenLoc));
        return false;
      }

      // IX or IY with displacement: (ix+d) or (ix-d)
      if ((Reg == Z80::IX || Reg == Z80::IY) &&
          (Parser.getTok().is(AsmToken::Plus) ||
           Parser.getTok().is(AsmToken::Minus))) {
        bool IsNeg = Parser.getTok().is(AsmToken::Minus);
        SMLoc PlusLoc = Parser.getTok().getLoc();
        Parser.Lex(); // Eat +/-

        const MCExpr *Disp;
        SMLoc DispLoc = Parser.getTok().getLoc();
        if (parseExpression(Disp))
          return true;

        if (IsNeg)
          Disp = MCUnaryExpr::createMinus(Disp, getContext());

        if (Parser.getTok().isNot(AsmToken::RParen))
          return Error(Parser.getTok().getLoc(), "expected ')'");
        SMLoc RParenLoc = Parser.getTok().getLoc();
        Parser.Lex(); // Eat ')'

        // Emit: "(" IX "+" disp ")"
        Operands.push_back(Z80Operand::CreateToken("(", LParenLoc));
        Operands.push_back(Z80Operand::CreateReg(Reg, RegLoc, RegEnd));
        Operands.push_back(Z80Operand::CreateToken("+", PlusLoc));
        Operands.push_back(Z80Operand::CreateImm(Disp, DispLoc, RParenLoc));
        Operands.push_back(Z80Operand::CreateToken(")", RParenLoc));
        return false;
      }

      // Simple register indirect: (hl), (bc), (de), (sp), (c), (ix), (iy)
      if (Parser.getTok().isNot(AsmToken::RParen))
        return Error(Parser.getTok().getLoc(), "expected ')'");
      SMLoc RParenEnd = Parser.getTok().getEndLoc();
      Parser.Lex(); // Eat ')'

      const char *CompTok = getCompoundToken(Reg);
      if (CompTok) {
        Operands.push_back(Z80Operand::CreateToken(CompTok, LParenLoc));
      } else {
        // Fallback: emit as separate tokens
        Operands.push_back(Z80Operand::CreateToken("(", LParenLoc));
        Operands.push_back(Z80Operand::CreateReg(Reg, RegLoc, RegEnd));
        Operands.push_back(Z80Operand::CreateToken(")", RParenEnd));
      }
      return false;
    }
  }

  // Direct addressing: (expr) — e.g., (0x1234) or (_label)
  Operands.push_back(Z80Operand::CreateToken("(", LParenLoc));

  SMLoc ExprLoc = Parser.getTok().getLoc();
  const MCExpr *Addr;
  if (parseExpression(Addr))
    return true;
  SMLoc ExprEnd =
      SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
  Operands.push_back(Z80Operand::CreateImm(Addr, ExprLoc, ExprEnd));

  if (Parser.getTok().isNot(AsmToken::RParen))
    return Error(Parser.getTok().getLoc(), "expected ')'");
  SMLoc RParenLoc = Parser.getTok().getLoc();
  Parser.Lex(); // Eat ')'

  Operands.push_back(Z80Operand::CreateToken(")", RParenLoc));
  return false;
}

/// Parse sdasz80 indexed addressing: d(ix) or d(iy).
/// The displacement expression has already been parsed.
/// Emits the same token sequence as LLVM format (ix+d):
///   "(" IX "+" disp ")"
bool Z80AsmParser::parseSDASZ80Indexed(OperandVector &Operands,
                                       const MCExpr *Disp, SMLoc DispLoc) {
  SMLoc LParenLoc = Parser.getTok().getLoc();
  Parser.Lex(); // Eat '('

  MCRegister Reg = tryParseRegisterName();
  if (!Reg || (Reg != Z80::IX && Reg != Z80::IY))
    return Error(Parser.getTok().getLoc(), "expected ix or iy register");

  SMLoc RegLoc = Parser.getTok().getLoc();
  SMLoc RegEnd = Parser.getTok().getEndLoc();
  Parser.Lex(); // Eat register

  if (Parser.getTok().isNot(AsmToken::RParen))
    return Error(Parser.getTok().getLoc(), "expected ')'");
  SMLoc RParenLoc = Parser.getTok().getLoc();
  Parser.Lex(); // Eat ')'

  // Emit as: "(" IX "+" disp ")"
  Operands.push_back(Z80Operand::CreateToken("(", LParenLoc));
  Operands.push_back(Z80Operand::CreateReg(Reg, RegLoc, RegEnd));
  Operands.push_back(Z80Operand::CreateToken("+", DispLoc));
  Operands.push_back(Z80Operand::CreateImm(Disp, DispLoc, RParenLoc));
  Operands.push_back(Z80Operand::CreateToken(")", RParenLoc));
  return false;
}

bool Z80AsmParser::parseOperand(OperandVector &Operands, StringRef Mnemonic) {
  LLVM_DEBUG(dbgs() << "parseOperand\n");

  // Handle '#' prefix: skip it, then parse the following as an immediate.
  // This handles sdasz80 syntax like "ld a, #5" or "ld hl, #_label".
  if (getLexer().is(AsmToken::Hash)) {
    Parser.Lex(); // Eat '#'

    SMLoc S = Parser.getTok().getLoc();
    const MCExpr *Expr;
    if (parseExpression(Expr))
      return true;
    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    Operands.push_back(Z80Operand::CreateImm(Expr, S, E));
    return false;
  }

  // Parenthesized operand: (reg), (expr), (ix+d)
  if (getLexer().is(AsmToken::LParen))
    return parseParenOperand(Operands);

  // Try register
  if (getLexer().is(AsmToken::Identifier)) {
    if (!tryParseRegisterOperand(Operands))
      return false;

    // Not a register — check for condition code tokens (nz, z, nc, po, pe, p, m).
    // These are used by conditional jr/jp/call/ret instructions and must be
    // emitted as Token operands for the AsmMatcher, not as expressions.
    // We must use string literals (not StringRef from .lower()) because
    // CreateToken stores a StringRef — the pointed-to data must be stable.
    {
      StringRef Name = Parser.getTok().getString();
      StringRef CC = StringSwitch<StringRef>(Name.lower())
                         .Case("nz", "nz")
                         .Case("nc", "nc")
                         .Case("po", "po")
                         .Case("pe", "pe")
                         .Case("z", "z")
                         .Case("p", "p")
                         .Case("m", "m")
                         .Default("");
      if (!CC.empty()) {
        SMLoc S = Parser.getTok().getLoc();
        Operands.push_back(Z80Operand::CreateToken(CC, S));
        Parser.Lex(); // Eat condition code
        return false;
      }
    }

    // Not a register or condition code — parse as expression/symbol
    SMLoc S = Parser.getTok().getLoc();
    const MCExpr *Expr;
    if (parseExpression(Expr))
      return true;
    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

    // Check for sdasz80 indexed format: expr(ix) or expr(iy)
    if (getLexer().is(AsmToken::LParen))
      return parseSDASZ80Indexed(Operands, Expr, S);

    Operands.push_back(Z80Operand::CreateImm(Expr, S, E));
    return false;
  }

  // Numeric immediate or expression (possibly followed by sdasz80 indexed)
  if (getLexer().is(AsmToken::Integer) || getLexer().is(AsmToken::Minus) ||
      getLexer().is(AsmToken::Plus)) {
    SMLoc S = Parser.getTok().getLoc();
    const MCExpr *Expr;
    if (parseExpression(Expr))
      return true;
    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

    // Check for sdasz80 indexed format: 5(ix) or -3(iy)
    if (getLexer().is(AsmToken::LParen))
      return parseSDASZ80Indexed(Operands, Expr, S);

    Operands.push_back(Z80Operand::CreateImm(Expr, S, E));
    return false;
  }

  return Error(Parser.getTok().getLoc(), "unexpected token in operand");
}

void Z80AsmParser::eatComma() {
  if (getLexer().is(AsmToken::Comma))
    Parser.Lex();
}

bool Z80AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                 SMLoc &EndLoc) {
  StartLoc = Parser.getTok().getLoc();
  Reg = tryParseRegisterName();
  EndLoc = Parser.getTok().getEndLoc();

  if (Reg) {
    Parser.Lex();
    return false;
  }
  return true;
}

ParseStatus Z80AsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                           SMLoc &EndLoc) {
  StartLoc = Parser.getTok().getLoc();
  Reg = tryParseRegisterName();
  EndLoc = Parser.getTok().getEndLoc();

  if (!Reg)
    return ParseStatus::NoMatch;

  Parser.Lex();
  return ParseStatus::Success;
}

bool Z80AsmParser::parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                                    SMLoc NameLoc, OperandVector &Operands) {
  // Add the mnemonic as the first operand
  Operands.push_back(Z80Operand::CreateToken(Name, NameLoc));

  // Parse operands
  int OperandNum = 0;
  while (getLexer().isNot(AsmToken::EndOfStatement)) {
    if (OperandNum > 0)
      eatComma();

    if (parseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.eatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }
    OperandNum++;
  }

  Parser.Lex(); // Consume the EndOfStatement

  // Handle ALU instructions where the A register is implicit in the
  // instruction definition. Instructions like AND, OR, XOR, SUB, CP don't
  // include "a," in their AsmString (e.g., AND_n is just "and $imm"),
  // but both LLVM and sdasz80 syntax commonly write "and a, 0x0F".
  // If we see mnemonic + A register + more operands, strip the A register.
  if (Operands.size() >= 3) {
    std::string Mne = static_cast<Z80Operand &>(*Operands[0]).getToken().lower();
    if (Mne == "and" || Mne == "or" || Mne == "xor" || Mne == "sub" ||
        Mne == "cp") {
      Z80Operand &FirstOp = static_cast<Z80Operand &>(*Operands[1]);
      if (FirstOp.isReg() && FirstOp.getReg() == Z80::A) {
        Operands.erase(Operands.begin() + 1);
      }
    }
  }

  return false;
}

/// Parse sdasz80 directives that differ from standard GNU as.
/// Currently handles:
///   .area _CODE  → .section .text
///   .area _DATA  → .section .data
///   .area _BSS   → .section .bss
ParseStatus Z80AsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getString();
  if (IDVal.lower() != ".area")
    return ParseStatus::NoMatch;

  // Parse area name
  if (getLexer().isNot(AsmToken::Identifier))
    return Error(getLexer().getLoc(), "expected area name after .area");

  StringRef AreaName = Parser.getTok().getString();
  Parser.Lex(); // Eat area name

  // Ignore optional flags like "(ABS)" or "(REL,CON)"
  if (getLexer().is(AsmToken::LParen)) {
    while (getLexer().isNot(AsmToken::EndOfStatement))
      Parser.Lex();
  }

  // Map sdasz80 area names to ELF sections
  StringRef Section;
  if (AreaName == "_CODE" || AreaName == "_HOME")
    Section = ".text";
  else if (AreaName == "_DATA" || AreaName == "_INITIALIZED")
    Section = ".data";
  else if (AreaName == "_BSS")
    Section = ".bss";
  else if (AreaName == "_GSINIT" || AreaName == "_GSFINAL")
    Section = ".init";
  else
    Section = ".text"; // default fallback

  getStreamer().switchSection(getContext().getELFSection(
      Section, Section == ".bss" ? ELF::SHT_NOBITS : ELF::SHT_PROGBITS,
      Section == ".bss"
          ? (ELF::SHF_ALLOC | ELF::SHF_WRITE)
          : Section == ".data"
                ? (ELF::SHF_ALLOC | ELF::SHF_WRITE)
                : (ELF::SHF_ALLOC | ELF::SHF_EXECINSTR)));

  return ParseStatus::Success;
}

bool Z80AsmParser::emit(MCInst &Inst, SMLoc const &Loc, MCStreamer &Out) const {
  Inst.setLoc(Loc);
  Out.emitInstruction(Inst, *STI);
  return false;
}

bool Z80AsmParser::invalidOperand(SMLoc const &Loc,
                                  OperandVector const &Operands,
                                  uint64_t const &ErrorInfo) {
  SMLoc ErrorLoc = Loc;

  if (ErrorInfo != ~0U) {
    if (ErrorInfo >= Operands.size()) {
      return Error(ErrorLoc, "too few operands for instruction");
    } else {
      Z80Operand const &Op =
          static_cast<Z80Operand const &>(*Operands[ErrorInfo]);
      if (Op.getStartLoc() != SMLoc())
        ErrorLoc = Op.getStartLoc();
    }
  }

  return Error(ErrorLoc, "invalid operand for instruction");
}

bool Z80AsmParser::missingFeature(SMLoc const &Loc, uint64_t const &ErrorInfo) {
  return Error(Loc, "instruction requires a CPU feature not currently enabled");
}

bool Z80AsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                           OperandVector &Operands,
                                           MCStreamer &Out, uint64_t &ErrorInfo,
                                           bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  switch (MatchResult) {
  case Match_Success:
    return emit(Inst, IDLoc, Out);
  case Match_MissingFeature:
    return missingFeature(IDLoc, ErrorInfo);
  case Match_InvalidOperand:
    return invalidOperand(IDLoc, Operands, ErrorInfo);
  case Match_MnemonicFail:
    return Error(IDLoc, "invalid instruction");
  case Match_immediate:
    return Error(IDLoc, "immediate operand out of range");
  default:
    return true;
  }
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80AsmParser() {
  RegisterMCAsmParser<Z80AsmParser> X(getTheZ80Target());
  RegisterMCAsmParser<Z80AsmParser> Y(getTheSM83Target());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "Z80GenAsmMatcher.inc"

// Defined after Z80GenAsmMatcher.inc to use generated matchTokenString/isSubclass.
unsigned Z80AsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                  unsigned ExpectedKind) {
  Z80Operand &Op = static_cast<Z80Operand &>(AsmOp);

  // Allow small non-negative immediate constants to match token operands for
  // bit/set/res/rst instructions. E.g., "bit 7, h" where 7 is parsed as
  // Immediate but the AsmMatcher expects token "7".
  // Only values 0-7 are valid bit positions; RST uses 0x00-0x38.
  if (Op.isImm() && ExpectedKind <= MCK_LAST_TOKEN) {
    if (const auto *CE = dyn_cast<MCConstantExpr>(Op.getImm())) {
      int64_t Val = CE->getValue();
      if (Val >= 0 && Val <= 0x38) {
        SmallString<8> Str;
        raw_svector_ostream OS(Str);
        OS << Val;
        if (isSubclass(matchTokenString(Str), (MatchClassKind)ExpectedKind))
          return Match_Success;
      }
    }
  }

  return Match_InvalidOperand;
}
