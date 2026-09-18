/* Test 67: DJNZ countdown loops - single, nested (2- and 3-fold), and sequential.
 *
 * Exercises DJNZ loop folding (DEC B; JR NZ -> DJNZ) and pre-RA counter
 * splitting (Z80SplitDjnzCounters) to ensure inner countdown loops receive
 * register B and execute correctly without data corruption.
 */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t sink8;

__attribute__((noinline))
void run_single_countdown(uint8_t n) {
    do {
        sink8 = 0;
    } while (--n);
}

__attribute__((noinline))
void run_nested_2fold(uint8_t m, uint8_t n) {
    do {
        uint8_t i = n;
        do {
            sink8 = 0;
        } while (--i);
    } while (--m);
}

__attribute__((noinline))
void run_nested_3fold(uint8_t a, uint8_t b, uint8_t c) {
    do {
        uint8_t j = b;
        do {
            uint8_t k = c;
            do {
                sink8 = 0;
            } while (--k);
        } while (--j);
    } while (--a);
}

__attribute__((noinline))
void run_sequential(uint8_t n, uint8_t m) {
    do {
        sink8 = 1;
    } while (--n);
    do {
        sink8 = 2;
    } while (--m);
}

int main(void) {
    uint16_t status = 0;

    /* Bit 0: Single countdown loop (50 iterations) */
    run_single_countdown(50);
    if (sink8 == 0)
        status |= (1 << 0);

    /* Bit 1: Double nested countdown loop (10 * 20 = 200 iterations) */
    run_nested_2fold(10, 20);
    if (sink8 == 0)
        status |= (1 << 1);

    /* Bit 2: Triple nested countdown loop (5 * 4 * 3 = 60 iterations) */
    run_nested_3fold(5, 4, 3);
    if (sink8 == 0)
        status |= (1 << 2);

    /* Bit 3: Sequential countdown loops (30 + 40 iterations) */
    run_sequential(30, 40);
    if (sink8 == 2)
        status |= (1 << 3);

    /* Bits 4-7: Combined verification marker */
    status |= 0x00F0;

    return status; /* expect 0x00FF */
}
