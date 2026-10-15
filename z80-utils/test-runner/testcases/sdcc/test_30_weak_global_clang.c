/* Test 30: weak definitions in the sdasz80 format.  The initial value of weak
 * data survives startup, and SDCC code sees the symbol.
 * expect 0x0007 */
typedef unsigned short u16;

volatile u16 weak_data __attribute__((weak)) = 0x1234;
__attribute__((weak)) u16 weak_func(void) { return 7; }

extern u16 sdcc_read_weak_data(void);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: the initial value is in place at startup. */
    if (weak_data == 0x1234)
        status |= (1 << 0);

    /* Bit 1: SDCC code reads the same variable. */
    if (sdcc_read_weak_data() == 0x1234)
        status |= (1 << 1);

    /* Bit 2: a weak function is called like any other. */
    if (weak_func() == 7)
        status |= (1 << 2);

    return status;
}
