// RUN: %clang_cc1 -triple z80 -O2 -S -o - %s | FileCheck %s

// Verify that a C countdown loop where the accumulator is modified in the body
// produces the 'LD A, B; DEC A; LD B, A; OR A; JR NZ' sequence before peephole
// and is folded into DJNZ.

volatile unsigned char sink8;

// CHECK-LABEL: _countdown_with_body:
// CHECK:       ld	b,{{ *}}a
// CHECK:       xor	a
// CHECK:       ld	(de),{{ *}}a
// CHECK-NEXT:  djnz	[[LOOP:\.LBB[0-9_]+]]
// CHECK:       ret
void countdown_with_body(unsigned char n) {
    do {
        sink8 = 0;
    } while (--n);
}
