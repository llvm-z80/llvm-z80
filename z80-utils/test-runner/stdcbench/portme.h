/* portme.h for the llvm-z80 test harness.
 *
 * Only c90base is enabled. c90lib needs a hosted C library (malloc, qsort,
 * str*, strtol) that this freestanding target does not have, and upstream
 * has not implemented c90float or c90double yet. */

typedef unsigned long stdcbench_clock_t;

#define STDCBENCH_CLOCKS_PER_SEC 100

#define C90BASE
#undef C90FLOAT
#undef C90DOUBLE
#undef C90LIB

/* c90base loops until STDCBENCH_CLOCKS_PER_SEC * 8 clock units have passed.
 * The harness supplies a synthetic clock that advances by exactly this much
 * divided by the iteration count, so the run does a fixed amount of work
 * instead of racing a wall clock the emulator does not have. */
#define STDCBENCH_C90BASE_UNITS (STDCBENCH_CLOCKS_PER_SEC * 8ul)

#ifndef STDCBENCH_ITERATIONS
#define STDCBENCH_ITERATIONS 1
#endif
