; RUN: llc -verify-machineinstrs -mtriple=z80 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -z80-asm-format=sdasz80 -O1 < %s | FileCheck %s --check-prefix=SM83
;
; A run of instructions that repeats is worth three bytes of CALL and one of
; RET. The call is smaller and slower than what it replaces, so only minsize
; takes the trade; a run reached through IX survives the CALL because the two
; bytes of return address it pushes move SP, not IX.

declare zeroext i16 @g(i16 zeroext)

define zeroext i16 @f1(i16 zeroext %a, i16 zeroext %b) minsize "frame-pointer"="all" {
entry:
  %xor = xor i16 %a, 4660
  %call = tail call zeroext i16 @g(i16 zeroext %xor)
  %xor1 = xor i16 %b, 4660
  %call2 = tail call zeroext i16 @g(i16 zeroext %xor1)
  %and = and i16 %a, 3855
  %and4 = and i16 %b, 3855
  %add = add i16 %and4, %and
  %add3 = add i16 %add, %call
  %add5 = add i16 %add3, %call2
  ret i16 %add5
}

define zeroext i16 @f2(i16 zeroext %a, i16 zeroext %b) minsize "frame-pointer"="all" {
entry:
  %xor = xor i16 %b, 4660
  %call = tail call zeroext i16 @g(i16 zeroext %xor)
  %xor1 = xor i16 %a, 4660
  %call2 = tail call zeroext i16 @g(i16 zeroext %xor1)
  %and = and i16 %b, 3855
  %and4 = and i16 %a, 3855
  %add = add i16 %and, %and4
  %add3 = add i16 %add, %call
  %add5 = add i16 %add3, %call2
  ret i16 %add5
}

define zeroext i16 @f3(i16 zeroext %a, i16 zeroext %b) optsize "frame-pointer"="all" {
entry:
  %or = or i16 %a, 21845
  %call = tail call zeroext i16 @g(i16 zeroext %or)
  %or1 = or i16 %b, 21845
  %call2 = tail call zeroext i16 @g(i16 zeroext %or1)
  %sub = sub i16 %call, %call2
  ret i16 %sub
}

define zeroext i16 @f4(i16 zeroext %a, i16 zeroext %b) optsize "frame-pointer"="all" {
entry:
  %or = or i16 %b, 21845
  %call = tail call zeroext i16 @g(i16 zeroext %or)
  %or1 = or i16 %a, 21845
  %call2 = tail call zeroext i16 @g(i16 zeroext %or1)
  %sub = sub i16 %call, %call2
  ret i16 %sub
}

; Z80-LABEL: _f1:
; Z80:       call _OUTLINED_FUNCTION_0
; Z80-LABEL: _f2:
; Z80:       call _OUTLINED_FUNCTION_0

; The trade spends speed on size, so optsize alone does not take it.

; Z80-LABEL: _f3:
; Z80-NOT:   call _OUTLINED
; Z80-LABEL: _f4:
; Z80-NOT:   call _OUTLINED
; Z80-LABEL: _OUTLINED_FUNCTION_0:
; Z80:       ret

; SM83 reaches its locals through SP, which the CALL moves. LDHL SP,e carries
; its own displacement, so the body asks for two more and reaches the same
; byte.

; SM83-LABEL: _f1:
; SM83:       call _OUTLINED_FUNCTION_0
; SM83-LABEL: _f2:
; SM83:       call _OUTLINED_FUNCTION_0
; SM83-LABEL: _f3:
; SM83-NOT:   call _OUTLINED
; SM83-LABEL: _f4:
; SM83-NOT:   call _OUTLINED
; SM83-LABEL: _OUTLINED_FUNCTION_0:
; SM83:       ldhl sp,#4
; SM83:       ret
