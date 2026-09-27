; RUN: llc -mtriple=z80 -O2 -stop-after=instruction-select < %s | FileCheck %s --check-prefix=MIR
; RUN: llc -verify-machineinstrs -mtriple=z80 -O2 < %s | FileCheck %s --check-prefix=Z80
; RUN: llc -verify-machineinstrs -mtriple=sm83 -O2 < %s | FileCheck %s --check-prefix=SM83
;
; The 8-bit ALU works on A. Selection gives the accumulator operand and the
; result virtual registers in Ac, reached through copies, rather than pinning
; them to A; the allocator then keeps a chain of operations in A and moves a
; result out only where it has to go. INC, which works in any register, joins
; the chain in A here because that is where its neighbours are.

; MIR-LABEL: name: chain
; MIR:       [[IN:%[0-9]+]]:ac = COPY
; MIR-NEXT:  [[OUT:%[0-9]+]]:ac = ADD_Ac_r [[IN]], {{%[0-9]+}}
; MIR-NEXT:  {{%[0-9]+}}:gr8 = COPY [[OUT]]
; MIR:       XOR_Ac_r
; MIR:       AND_Ac_n {{%[0-9]+}}, 15
; MIR:       {{%[0-9]+}}:gr8 = INC_r

; Z80-LABEL: _chain:
; Z80:       add a,l
; Z80-NEXT:  xor b
; Z80-NEXT:  and 15
; Z80-NEXT:  inc a
; SM83-LABEL: _chain:
; SM83:       add a,e
; SM83-NEXT:  xor b
; SM83-NEXT:  and 15
; SM83-NEXT:  inc a
define i8 @chain(i8 %a, i8 %b, i8 %c) {
  %x = add i8 %a, %b
  %y = xor i8 %x, %c
  %z = and i8 %y, 15
  %w = add i8 %z, 1
  ret i8 %w
}

; Each byte of the pair is computed in A and goes straight to its half of the
; argument pair. Copy propagation in MachineCSE hands the operations the
; argument bytes themselves and narrows them to Ac; were the two left in A at
; once, the allocator would split one and route it through a third register.

; SM83-LABEL: _pair_arg:
; SM83:       ld a,e
; SM83-NEXT:  xor 52
; SM83-NEXT:  ld e,a
; SM83-NEXT:  ld a,d
; SM83-NEXT:  xor 18
; SM83-NEXT:  ld d,a
; SM83-NEXT:  call _g
declare i16 @g(i16)

define i16 @pair_arg(i16 %a) {
  %x = xor i16 %a, 4660
  %r = tail call i16 @g(i16 %x)
  ret i16 %r
}

; INC and DEC work in any register, and DEC sets Z for what it leaves, so a
; byte counter steps and is tested where it lives: the zero test that would
; have copied it into A is dropped before allocation.

; Z80-LABEL: _count:
; Z80:       ld (hl),a
; Z80-NEXT:  dec c
; Z80-NEXT:  inc hl
; Z80-NEXT:  jr nz,
; SM83-LABEL: _count:
; SM83:       ld (de),a
; SM83-NEXT:  dec c
; SM83-NEXT:  inc de
; SM83-NEXT:  jr nz,
define void @count(ptr %p, i8 %n) {
entry:
  br label %loop
loop:
  %i = phi i8 [0, %entry], [%i1, %loop]
  %q = getelementptr i8, ptr %p, i8 %i
  %v = load i8, ptr %q
  %v2 = add i8 %v, %n
  %v3 = or i8 %v2, 128
  store i8 %v3, ptr %q
  %i1 = add i8 %i, 1
  %c = icmp ne i8 %i1, 16
  br i1 %c, label %loop, label %exit
exit:
  ret void
}

; Clearing or setting one bit is RES or SET, and flipping all of them CPL,
; none of which ties the byte to A or writes flags nobody reads.

; MIR-LABEL: name: bits
; MIR:       RES_b_r 7,
; MIR:       SET_b_r 0,
; MIR:       CPL_Ac
; SM83-LABEL: _bits:
; SM83:       res 7,a
; SM83:       set 0,a
; SM83:       cpl
define void @bits(ptr %p, i8 %x) {
  %a = and i8 %x, 127
  store i8 %a, ptr %p
  %q = getelementptr i8, ptr %p, i16 1
  %o = or i8 %x, 1
  store i8 %o, ptr %q
  %r = getelementptr i8, ptr %p, i16 2
  %n = xor i8 %x, -1
  store i8 %n, ptr %r
  ret void
}

; A byte spilled across the call is read back by the ALU straight from its
; slot: the allocator folds the reload into the accumulator operation.

; Z80-LABEL: _spill_fold:
; Z80:       call _clobber
; Z80:       add a,(ix+-1)
declare void @clobber()

define i8 @spill_fold(i8 %a, i8 %b) "frame-pointer"="all" {
  call void @clobber()
  %s = add i8 %a, %b
  %t = xor i8 %s, %a
  ret i8 %t
}

; The borrow out of a 16-bit subtraction is its own 0/1 value once captured,
; so neither the mask the widened branch condition asks for nor a test
; before the jump is left.

; Z80-LABEL: _borrow_branch:
; Z80:       sbc hl,de
; Z80-NEXT:  sbc a,a
; Z80-NEXT:  and 1
; Z80-NEXT:  xor 1
; Z80-NEXT:  jr nz,
declare void @use16(i16)

define void @borrow_branch(i16 %a, i16 %b) {
  %r = call { i16, i1 } @llvm.usub.with.overflow.i16(i16 %a, i16 %b)
  %v = extractvalue { i16, i1 } %r, 0
  %o = extractvalue { i16, i1 } %r, 1
  br i1 %o, label %under, label %done
under:
  call void @use16(i16 %v)
  ret void
done:
  ret void
}
