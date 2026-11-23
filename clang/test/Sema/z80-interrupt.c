// RUN: %clang_cc1 -triple z80 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple sm83 -fsyntax-only -verify=expected,sm83 %s

__attribute__((interrupt)) void isr(void) {}
__attribute__((interrupt("nested"))) void isr_nested(void) {}
__attribute__((interrupt("nmi"))) void isr_nmi(void) {} // sm83-warning {{'interrupt' attribute argument not supported: nmi}}
__attribute__((interrupt("fast"))) void isr_fast(void) {} // expected-warning {{'interrupt' attribute argument not supported: fast}}
__attribute__((interrupt("nested", "nmi"))) void isr_two(void) {} // expected-error {{'interrupt' attribute takes no more than 1 argument}}
__attribute__((interrupt(1))) void isr_number(void) {} // expected-error {{expected string literal as argument of 'interrupt' attribute}}
__attribute__((interrupt)) void isr_param(int x) {} // expected-warning {{Z80 'interrupt' attribute only applies to functions that have no parameters}}
__attribute__((interrupt)) int isr_result(void) { return 0; } // expected-warning {{Z80 'interrupt' attribute only applies to functions that have a 'void' return type}}
int var __attribute__((interrupt)); // expected-warning {{'interrupt' attribute only applies to functions}}
