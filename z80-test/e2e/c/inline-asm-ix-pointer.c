// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0003 %t
// UNSUPPORTED: O0, sm83

/* Inline asm outputs pinned to IX, used as pointers after a later asm writes
 * IX. At -O0, IX is the frame pointer and the asm cannot write IX. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t ga = 3;
volatile uint8_t gb[2] = {5, 7};

__attribute__((noinline)) uint8_t sum_two(void) {
    register volatile uint8_t *p __asm__("ix");
    register volatile uint8_t *q __asm__("ix");
    __asm__ volatile("ld ix, #_ga" : "=r"(p));
    __asm__ volatile("ld ix, #_gb" : "=r"(q));
    return *p + q[1];
}

__attribute__((noinline)) uint8_t after_clobber(void) {
    register volatile uint8_t *p __asm__("ix");
    __asm__ volatile("ld ix, #_gb" : "=r"(p));
    __asm__ volatile("ld ix, #_ga" : : : "ix");
    return p[1];
}

int main() {
    uint16_t status = 0;

    if (sum_two() == 10)
        status |= 1 << 0;
    if (after_clobber() == 7)
        status |= 1 << 1;

    return status;
}
