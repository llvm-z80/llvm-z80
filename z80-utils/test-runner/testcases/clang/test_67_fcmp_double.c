/* Test 67: FCMP double comparison libcall return width.
 *
 * GCC soft-float ABI specifies that comparison libcalls (__eqdf2, ...) return
 * a C int (16-bit on Z80 in register DE).
 * If the compiler assumes a 32-bit return (HL:DE), reading HL picks up callee
 * garbage and causes equality tests to fail spuriously when HL != 0.
 */
/* expect: 0x0003 */

/* Provide stubs for __eqdf2 and __nedf2 that return 0 in DE (equal), but
 * leave HL non-zero (0x1234) simulating uninitialized callee register state.
 * a == b calls __eqdf2; a != b calls __nedf2. */
int __eqdf2(double a, double b) {
    (void)a;
    (void)b;
    __asm__ volatile("ld hl, 0x1234" ::: "hl");
    return 0;
}

int __nedf2(double a, double b) {
    (void)a;
    (void)b;
    __asm__ volatile("ld hl, 0x1234" ::: "hl");
    return 0;
}

volatile double a = 3.0;
volatile double b = 3.0;

int main(void) {
    int status = 0;

    /* Bit 0: a == b must evaluate to true (DE is 0; HL garbage must be ignored) */
    if (a == b) {
        status |= (1 << 0);
    }

    /* Bit 1: a != b must evaluate to false */
    if (!(a != b)) {
        status |= (1 << 1);
    }

    return status; /* expect 0x0003 */
}
