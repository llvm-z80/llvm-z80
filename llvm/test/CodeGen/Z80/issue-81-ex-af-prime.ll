; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O0 < %s | FileCheck %s
; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O2 < %s | FileCheck %s

; ravn/llvm-z80#81 — the integrated assembler must accept "ex af, af'"
; in inline asm.  The AsmLexer treats apostrophe as a single-quoted
; string opener, so historically "af'\n..." was mis-tokenised as
; identifier "af" plus an unterminated string literal, and the only
; workaround was to encode the byte directly with .byte 0x08.
;
; The fix detects the af-followed-by-apostrophe pattern in
; tryParseRegisterOperand, pushes a Token "af'" operand to match the
; auto-generated MCK_af_39_ class for EX_AF_AF, and repositions the
; lexer past the apostrophe so the rest of the line lexes normally.

; CHECK-LABEL: _ex_af_lower:
; CHECK:       ex	af,af'
; CHECK:       ret
define void @ex_af_lower() naked {
  call void asm sideeffect "ex af, af'", ""()
  ret void
}

; CHECK-LABEL: _ex_af_upper:
; CHECK:       ex	af,af'
; CHECK:       ret
define void @ex_af_upper() naked {
  call void asm sideeffect "EX AF, AF'", ""()
  ret void
}

; CHECK-LABEL: _ex_af_no_space:
; CHECK:       ex	af,af'
; CHECK:       ret
define void @ex_af_no_space() naked {
  call void asm sideeffect "ex af,af'", ""()
  ret void
}

; Mixed sequence: ex af,af' followed by other instructions on the
; same inline-asm string.  Confirms the lexer is properly repositioned
; past the apostrophe so subsequent tokens lex normally.
; CHECK-LABEL: _ex_af_then_exx:
; CHECK:       ex	af,af'
; CHECK:       exx
; CHECK:       ex	de,hl
; CHECK:       ret
define void @ex_af_then_exx() naked {
  call void asm sideeffect "ex af, af'\0A\09exx\0A\09ex de, hl", ""()
  ret void
}
