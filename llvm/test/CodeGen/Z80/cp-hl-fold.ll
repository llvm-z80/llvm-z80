; RUN: llc -verify-machineinstrs -mtriple=z80 -O1 < %s | FileCheck %s
;
; A byte compared with a byte in memory should use CP (HL), where the old
; sequence was LD B,A; LD A,(DE); LD C,A; LD A,B; CP C. The load folds into
; the compare when the loaded byte has no other use and the compare only
; feeds a branch (the flags are all that is wanted).
;
; The C these functions come from (clang -O1, z80), with an executable
; version in z80-test/e2e/c/cp-hl-fold.c:
;
;   typedef unsigned char u8;
;   void g(void);
;   void eq_branch(u8 a, u8 *p) { if (a == *p) g(); }
;   void lt_branch(u8 a, u8 *p) { if (a < *p) g(); }
;   u8 eq_value(u8 a, u8 *p) { return a == *p; }
;   u8 two_uses(u8 a, u8 *p) { u8 v = *p; if (a == v) g(); return v; }

declare void @g()

; Equality branch: the load folds.
define void @eq_branch(i8 zeroext %a, ptr %p) {
; CHECK-LABEL: eq_branch:
; CHECK-NOT:     ld a,(
; CHECK:         cp (hl)
; CHECK-NEXT:    jp z,_g
  %v = load i8, ptr %p, align 1
  %cmp = icmp eq i8 %a, %v
  br i1 %cmp, label %then, label %end
then:
  tail call void @g()
  br label %end
end:
  ret void
}

; Ordering branch: the same fold through the carry flag.
define void @lt_branch(i8 zeroext %a, ptr %p) {
; CHECK-LABEL: lt_branch:
; CHECK-NOT:     ld a,(
; CHECK:         cp (hl)
; CHECK-NEXT:    jp c,_g
  %v = load i8, ptr %p, align 1
  %cmp = icmp ult i8 %a, %v
  br i1 %cmp, label %then, label %end
then:
  tail call void @g()
  br label %end
end:
  ret void
}

; Control: a compare that produces a value needs the difference in A, so it
; still loads the byte (CP (HL) would not leave the difference).
define i8 @eq_value(i8 zeroext %a, ptr %p) {
; CHECK-LABEL: eq_value:
; CHECK-NOT:     cp (hl)
; CHECK:         ld a,(de)
  %v = load i8, ptr %p, align 1
  %cmp = icmp eq i8 %a, %v
  %conv = zext i1 %cmp to i8
  ret i8 %conv
}

; Control: the loaded byte is used again after the compare, so it must be
; loaded into a register anyway and the fold would only add a second load.
define i8 @two_uses(i8 zeroext %a, ptr %p) {
; CHECK-LABEL: two_uses:
; CHECK-NOT:     cp (hl)
; CHECK:         ld a,(de)
  %v = load i8, ptr %p, align 1
  %cmp = icmp eq i8 %a, %v
  br i1 %cmp, label %then, label %end
then:
  tail call void @g()
  br label %end
end:
  ret i8 %v
}
