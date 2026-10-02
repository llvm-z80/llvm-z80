// RUN: %clang_cc1 -triple z80 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64 -emit-llvm -o - %s | FileCheck %s --check-prefix=X86
//
// A classic-ABI C library may declare a standard library function with an
// explicit Z80 calling convention that is byte-for-byte the ABI of the
// linked implementation -- e.g. fwrite is __attribute__((smallc)) (cc129:
// stack args, caller cleanup). fwrite is also a recognized C library
// builtin, and in
// hosted mode (the default; -ffreestanding NOT passed) clang's
// redeclaration-merge logic used to STRIP an explicit CC off a builtin and
// reset it to the default C convention (Sema::MergeFunctionDecl). On Z80
// that silently corrupts the ABI: fwrite() would then pass/return in the
// wrong registers/stack, producing a garbage result. The fix HONORS an
// explicit Z80 CC on such a redeclaration instead of resetting it.
//
// -O0 so no libcall simplification runs; the emitted call CC comes purely
// from the frontend merge, isolating the Sema behavior.

typedef struct _FILE FILE;

// EXACT bug pattern: fwrite (a builtin) declared smallc keeps cc129.
extern int fwrite(const void *, unsigned, unsigned, FILE *) __attribute__((smallc));

// CHECK-LABEL: define {{.*}}i16 @call_fwrite(
// CHECK:         call cc129 i16 @fwrite(
// X86-LABEL:   define {{.*}}@call_fwrite(
// X86-NOT:       cc129
int call_fwrite(FILE *f, const void *p) {
  return fwrite(p, 1, 128, f);
}

// POSITIVE CONTROL: a non-builtin function with no explicit CC is
// unaffected (goes through the plain-inherit path, not this exception).
extern int not_a_builtin(int);
// CHECK-LABEL: define {{.*}}i16 @call_worker(
// CHECK-NOT:     cc129
int call_worker(int x) { return not_a_builtin(x); }
