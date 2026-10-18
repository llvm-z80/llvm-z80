/* CoreMark port for the z80-run PGO corpus, after the barebones port in the
 * CoreMark sources (Copyright 2018 EEMBC, Apache License 2.0).
 *
 * CoreMark's main is built as coremark_main, so the main of this port can
 * return 0 only after CoreMark validates the run against the known CRCs. The
 * clock always reports ten seconds, since CoreMark counts a shorter run as an
 * error. */

#include "coremark.h"

/* The seeds of a 2K performance run, so that CoreMark can validate the run. */
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

ee_u32 default_num_contexts = 1;

#define EE_TICKS_PER_SEC 1000

static ee_u8 validated;
static ee_u8 failed;

void start_time(void) {}

void stop_time(void) {}

CORE_TICKS get_time(void)
{
    return 10 * EE_TICKS_PER_SEC;
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
    return ticks / EE_TICKS_PER_SEC;
}

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix)
        if (*s++ != *prefix++)
            return 0;
    return 1;
}

void coremark_note(const char *fmt)
{
    if (starts_with(fmt, "Correct operation validated"))
        validated = 1;
    if (starts_with(fmt, "Errors detected") || starts_with(fmt, "Cannot validate"))
        failed = 1;
}

void portable_init(core_portable *p, int *argc, char *argv[])
{
    (void)argc;
    (void)argv;
    p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
    p->portable_id = 0;
}

#undef main
int coremark_main(void);

int main(void)
{
    coremark_main();
    return validated && !failed ? 0 : 1;
}
