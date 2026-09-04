/* portme.c for the llvm-z80 test harness.
 *
 * Reports through memory rather than stdio: there is no C library here, and
 * the runner reads these symbols back out of the emulator's RAM dump. Names
 * stay within nine characters because the SDCC map file truncates there.
 *
 * The score stdcbench computes is not a stdcbench score. It is derived from
 * the synthetic clock below, so it says nothing about speed; the runner
 * measures cycles instead. What is meaningful is `berror`, which stdcbench
 * sets when one of its own result checks fails. */

#include "stdcbench.h"

volatile unsigned long score;
volatile unsigned char berror;

static stdcbench_clock_t elapsed;

void stdcbench_error(const char *message)
{
	(void)message;
	berror = 1;
}

/* Advances a fixed step per call, which makes c90base run exactly
 * STDCBENCH_ITERATIONS times regardless of how fast the code is. */
stdcbench_clock_t stdcbench_clock(void)
{
	stdcbench_clock_t now = elapsed;
	/* Round the step up. c90base stops once the elapsed count reaches
	 * STDCBENCH_C90BASE_UNITS, so a truncated step leaves the last
	 * iteration just short of the threshold and runs one extra. */
	elapsed += (STDCBENCH_C90BASE_UNITS + STDCBENCH_ITERATIONS - 1)
	           / STDCBENCH_ITERATIONS;
	return now;
}

int main(void)
{
	/* Set explicitly: the SDCC startup path does not clear .bss, and an
	 * uninitialized berror reads back as 0xFF and looks like a failure. */
	berror = 0;
	elapsed = 0;

	score = stdcbench();
	return berror;
}
