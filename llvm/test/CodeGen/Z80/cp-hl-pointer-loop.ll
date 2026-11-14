; RUN: llc -mtriple=z80 -O2 -verify-machineinstrs %s -o - | FileCheck %s
; RUN: llc -mtriple=z80 -O2 -stop-after=instruction-select -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=ISEL

; Issue #401: pointer-comparison loops should generate the optimal
;   LD A,(BC) ; CP (HL) ; INC HL ; INC BC ; DEC r ; JR NZ
; pattern — one pointer in BC (read via LOAD8_IND -> LD A,(BC)), the other
; in HL (compared via COMPARE8_IND -> CP (HL)), no accumulator spill, no
; IX frame. Enabled by:
;   - CP (HL) fold in commit 605546e1a4c4
;   - LSR cost-model hooks (isHardwareLoopProfitable, getNumberOfRegisters,
;     shouldDropLSRSolutionIfLessProfitable) that pressure LSR to choose
;     a narrow i8 down-counter + minimal IV set over its wide stride-match
;     default.

target triple = "z80"

define zeroext i8 @cmp6(ptr %a, ptr %b) {
entry:
  br label %do.body

do.body:
  %pa = phi ptr [ %a, %entry ], [ %pa.next, %do.cond ]
  %pb = phi ptr [ %b, %entry ], [ %pb.next, %do.cond ]
  %i  = phi i8  [ 6,  %entry ], [ %i.dec,   %do.cond ]
  %va = load i8, ptr %pa, align 1
  %vb = load i8, ptr %pb, align 1
  %ne = icmp ne i8 %va, %vb
  br i1 %ne, label %exit1, label %do.cond

do.cond:
  %pa.next = getelementptr inbounds i8, ptr %pa, i16 1
  %pb.next = getelementptr inbounds i8, ptr %pb, i16 1
  %i.dec = add i8 %i, -1
  %cont = icmp ne i8 %i.dec, 0
  br i1 %cont, label %do.body, label %exit0

exit1:
  ret i8 1

exit0:
  ret i8 0
}

; CHECK-LABEL: _cmp6:
; The loop body must contain the folded CP (HL) with no spill between
; the LD A,(..) and the compare.
; CHECK:       ld a,(bc)
; CHECK-NEXT:  cp (hl)
; CHECK-NEXT:  jr nz,
; CHECK-DAG:   inc hl
; CHECK-DAG:   inc bc
; CHECK-NOT:   ld ({{(ix|L_|iy)}}

; ISEL-LABEL: name: cmp6
; ISEL:        COMPARE8_IND
