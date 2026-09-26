// RUN: %clang_cc1 -triple z80 -O2 -S -o - %s | FileCheck %s

// Test mapping from C pointer dereference in address_space(2) to Z80 IN/OUT instructions.
// Compile-time-constant ports are lowered to the short IN A,(n) / OUT (n),A instructions.

#define __io __attribute__((address_space(2)))

// CHECK-LABEL: _test_port_out_const:
// CHECK:       out (16),a
// CHECK-NEXT:  ret
void test_port_out_const(unsigned char val) {
  *(volatile __io unsigned char *)0x10 = val;
}

// CHECK-LABEL: _test_port_in_const:
// CHECK:       in a,(32)
// CHECK-NEXT:  ret
unsigned char test_port_in_const(void) {
  return *(volatile __io unsigned char *)0x20;
}

// CHECK-LABEL: _test_port_out_zero:
// CHECK:       xor a
// CHECK-NEXT:  out (5),a
// CHECK-NEXT:  ret
void test_port_out_zero(void) {
  *(volatile __io unsigned char *)0x05 = 0;
}

// CHECK-LABEL: _test_port_poll_write:
// CHECK:       in a,(5)
// CHECK:       out (6),a
// CHECK-NEXT:  ret
void test_port_poll_write(unsigned char val) {
  volatile unsigned char status = *(volatile __io unsigned char *)0x05;
  (void)status;
  *(volatile __io unsigned char *)0x06 = val;
}
