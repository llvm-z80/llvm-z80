// RUN: %clang_cc1 -triple z80 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple sm83 -fsyntax-only -verify=expected,sm83 %s
// RUN: %clang_cc1 -triple x86_64 -fsyntax-only -verify=other %s

void keep_bc(void) __attribute__((preserves_regs(b, c))); // other-warning {{unknown attribute 'preserves_regs' ignored}}
void keep_all(void) __attribute__((preserves_regs(a, b, c, d, e, h, l))); // other-warning {{unknown attribute 'preserves_regs' ignored}}
[[clang::preserves_regs(b, c)]] void keep_std(void); // other-warning {{unknown attribute 'clang::preserves_regs' ignored}}

// SM83 has no IY.
void keep_iy(void) __attribute__((preserves_regs(iyl, iyh, b))); // sm83-warning {{'preserves_regs' attribute argument not supported: iyl}} \
                                                                 // sm83-warning {{'preserves_regs' attribute argument not supported: iyh}} \
                                                                 // other-warning {{unknown attribute 'preserves_regs' ignored}}

// SDCC's names only; SDCC warns about the others too.
void keep_pair(void) __attribute__((preserves_regs(bc, b))); // expected-warning {{'preserves_regs' attribute argument not supported: bc}} \
                                                             // other-warning {{unknown attribute 'preserves_regs' ignored}}
void keep_upper(void) __attribute__((preserves_regs(B))); // expected-warning {{'preserves_regs' attribute argument not supported: B}} \
                                                          // other-warning {{unknown attribute 'preserves_regs' ignored}}

void keep_none(void) __attribute__((preserves_regs)); // expected-error {{'preserves_regs' attribute takes at least 1 argument}} \
                                                      // other-warning {{unknown attribute 'preserves_regs' ignored}}
void keep_number(void) __attribute__((preserves_regs(1))); // expected-error {{'preserves_regs' attribute requires parameter 1 to be an identifier}} \
                                                           // other-warning {{unknown attribute 'preserves_regs' ignored}}

int not_a_function __attribute__((preserves_regs(b))); // expected-warning {{'preserves_regs' attribute only applies to functions}} \
                                                       // other-warning {{unknown attribute 'preserves_regs' ignored}}

// A definition can have the attribute.
void body(void) __attribute__((preserves_regs(b, c))) {} // other-warning {{unknown attribute 'preserves_regs' ignored}}

// A redeclaration inherits the list, or repeats the same registers in any order.
void redecl_inherit(void) __attribute__((preserves_regs(b, c))); // other-warning {{unknown attribute 'preserves_regs' ignored}}
void redecl_inherit(void) {}
void redecl_same(void) __attribute__((preserves_regs(b, c))); // other-warning {{unknown attribute 'preserves_regs' ignored}}
void redecl_same(void) __attribute__((preserves_regs(c, b, b))); // other-warning {{unknown attribute 'preserves_regs' ignored}}
void redecl_diff(void) __attribute__((preserves_regs(b, c))); // expected-note {{previous attribute is here}} \
                                                              // other-warning {{unknown attribute 'preserves_regs' ignored}}
void redecl_diff(void) __attribute__((preserves_regs(b))) {} // expected-error {{attribute 'preserves_regs' is already applied with different arguments}} \
                                                             // other-warning {{unknown attribute 'preserves_regs' ignored}}
