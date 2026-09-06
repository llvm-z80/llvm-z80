; RUN: llc -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s
;
; IMPLEMENTED (cc 131 = CallingConv::Z80_Z88dkCallee): __z88dk_callee.  Arguments
; are pushed on the stack exactly like __sdcccall(0), but the CALLEE cleans them
; up on return (Z80 has no `ret N`, so the backend uses the RET_CLEANUP pseudo).
; Return values use the z88dk classic registers (i8=L, i16=HL, i32=DE:HL), same
; as sdcccall(0).  Callee cleanup is forced regardless of return size (unlike
; sdcccall(1), which caller-cleans for >16-bit returns) -- see callee_reti32.
;
; The pivotal invariant is EXACTLY-ONCE cleanup: the callee pops the N argument
; bytes, and the caller does NOT.  A stale/no-op convention would either
; double-pop (corruption) or never pop (leak).
;
; This started life as an expected-failure test written before the backend
; support; the expected-failure directive was removed in the same changeset
; that wired up the convention (leaving it stale would make lit report XPASS and
; fail CI, the intended tripwire).

; ============================================================================
; (a) caller side -- pushes the args, does NOT clean up (the callee will)
; ============================================================================

declare cc 131 void @sink2(i16, i16)

; CHECK-LABEL: _call_callee:
; CHECK:      push hl
; CHECK:      push hl
; CHECK:      call _sink2
; CHECK-NOT:  pop
; CHECK-NOT:  inc sp
; CHECK:      ret
define void @call_callee() {
  call cc 131 void @sink2(i16 1, i16 2)
  ret void
}

; ============================================================================
; (b) callee side -- reads args off the stack frame and callee-cleans them
; ============================================================================

; void return (HL dead): args come from 4(ix)/6(ix); cleanup uses the EX trick
; (pop return addr into HL, drop the 4 arg bytes with inc sp x2, restore return
; addr with ex (sp),hl).
; CHECK-LABEL: _callee_void:
; CHECK:      4(ix)
; CHECK:      6(ix)
; CHECK:      pop hl
; CHECK:      inc sp
; CHECK:      inc sp
; CHECK:      ex (sp),hl
; CHECK-NEXT: ret
define cc 131 void @callee_void(i16 %a, i16 %b) {
  %s = add i16 %a, %b
  store i16 %s, ptr inttoptr(i16 16384 to ptr)
  ret void
}

; i16 return: value comes back in HL (not DE, so no `ex de,hl`), and because HL
; holds the return value the cleanup uses the BC fallback (pop return addr into
; BC, drop 4 arg bytes with inc sp x4, push BC back).
; CHECK-LABEL: _callee_reti16:
; CHECK:      4(ix)
; CHECK-NOT:  ex de,hl
; CHECK:      pop bc
; CHECK:      inc sp
; CHECK:      inc sp
; CHECK:      inc sp
; CHECK:      inc sp
; CHECK:      push bc
; CHECK-NEXT: ret
define cc 131 i16 @callee_reti16(i16 %a, i16 %b) {
  %s = add i16 %a, %b
  ret i16 %s
}

; i32 return: callee cleanup is FORCED even though the return is > 16 bits.
; sdcccall(1) would caller-clean here; cc 131 must still pop its own 4 arg
; bytes (BC-fallback pop/push).  Return value is in DE:HL.
; CHECK-LABEL: _callee_reti32:
; CHECK:      4(ix)
; CHECK:      pop bc
; CHECK:      push bc
; CHECK-NEXT: ret
define cc 131 i32 @callee_reti32(i16 %a, i16 %b) {
  %z = zext i16 %a to i32
  ret i32 %z
}

; ============================================================================
; (d) boundary -- a 0-argument callee-cleanup fn must NOT emit a spurious pop
; ============================================================================

; CHECK-LABEL: _callee_noargs:
; CHECK-NOT:  inc sp
; CHECK:      ret
define cc 131 void @callee_noargs() {
  ret void
}
