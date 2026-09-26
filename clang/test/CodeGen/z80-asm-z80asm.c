// RUN: %clang_cc1 -triple z80 -mllvm -z80-asm-format=z80asm -S -o - %s | FileCheck %s

// 1. Function in code_compiler section with GLOBAL directive and branch label
void external_call(void);
void test_func(char c) {
  if (c)
    external_call();
}

// CHECK:        SECTION code_compiler
// CHECK-NEXT:   GLOBAL	_test_func
// CHECK-LABEL: _test_func:
// CHECK-NOT:   $local
// CHECK:        jr	z, LBB0_
// CHECK:        call	_external_call
// CHECK-LABEL: LBB0_
// CHECK:        ret
// CHECK-NOT:   .Lfunc_end
// CHECK-NOT:   .size
// CHECK-NOT:   .type

// 2. Initialized global in data_compiler section
int global_data = 0x1234;

// CHECK:        SECTION data_compiler
// CHECK-NEXT:   GLOBAL	_global_data
// CHECK-LABEL: _global_data:
// CHECK-NEXT:   DEFW	4660

// 3. 32-bit and 64-bit integer globals (64-bit split into two DEFQs)
long global_quad = 0x12345678L;
long long global_i64 = 0x0123456789ABCDEFull;

// CHECK-NEXT:   GLOBAL	_global_quad
// CHECK-LABEL: _global_quad:
// CHECK-NEXT:   DEFQ	305419896

// CHECK-NEXT:   GLOBAL	_global_i64
// CHECK-LABEL: _global_i64:
// CHECK-NEXT:   DEFQ	2309737967
// CHECK-NEXT:   DEFQ	19088743

// 4. Zero-initialized global in bss_compiler section
int global_bss[4];

// CHECK:        SECTION bss_compiler
// CHECK-NEXT:   GLOBAL	_global_bss
// CHECK-LABEL: _global_bss:
// CHECK-NEXT:   DEFS	8

// 5. String literal in rodata_compiler section and dotless string symbol
const char *get_string(void) {
  return "hello";
}

// CHECK:        SECTION rodata_compiler
// CHECK-LABEL: L__str:
// CHECK-NEXT:   DEFM	"hello\000"

// 6. Local static variables with dotless mangled names
int test_static(void) {
  static int counter = 42;
  return counter++;
}

// CHECK:        SECTION data_compiler
// CHECK-NOT:   _counter.
// CHECK:       _test_static_counter:
// CHECK-NEXT:   DEFW	42

// 7. Verify no ELF directives or sections anywhere in output
// CHECK-NOT:   .text
// CHECK-NOT:   .data
// CHECK-NOT:   .bss
// CHECK-NOT:   .rodata
// CHECK-NOT:   .globl
// CHECK-NOT:   .file
// CHECK-NOT:   .ident
// CHECK-NOT:   .note.GNU-stack
