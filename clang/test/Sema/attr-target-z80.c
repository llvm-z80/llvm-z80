// RUN: %clang_cc1 -triple z80 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple sm83 -fsyntax-only -verify %s

// The subtarget features that may be spelled in a target attribute. A name
// outside this set is reported here rather than reaching the backend, where it
// would be dropped and leave the attribute doing nothing.

__attribute__((target("z80"))) void f_z80(void) {}
__attribute__((target("z180"))) void f_z180(void) {}
__attribute__((target("r800"))) void f_r800(void) {}
__attribute__((target("ez80"))) void f_ez80(void) {}
__attribute__((target("sm83"))) void f_sm83(void) {}
__attribute__((target("undocumented"))) void f_undoc(void) {}
__attribute__((target("static-frame"))) void f_frame(void) {}
__attribute__((target("no-static-frame"))) void f_no_frame(void) {}
__attribute__((target("inline-i16-runtime"))) void f_inline_rt(void) {}

// A misspelling of the one attribute that keeps a function's frame on the
// stack must not pass for the attribute itself.
// expected-warning@+1 {{unsupported 'static-frames' in the 'target' attribute string; 'target' attribute ignored}}
__attribute__((target("no-static-frames"))) void f_typo(void) {}

// expected-warning@+1 {{unsupported 'staticframe' in the 'target' attribute string; 'target' attribute ignored}}
__attribute__((target("no-staticframe"))) void f_run_together(void) {}

// expected-warning@+1 {{unsupported 'nonsense' in the 'target' attribute string; 'target' attribute ignored}}
__attribute__((target("nonsense"))) void f_junk(void) {}

// A processor name is not a feature name, and naming one as a feature is the
// same mistake.
// expected-warning@+1 {{unsupported 'gameboy' in the 'target' attribute string; 'target' attribute ignored}}
__attribute__((target("gameboy"))) void f_cpu_as_feature(void) {}
