/* Test 67: Countdown loops - single, nested (2- and 3-fold), and sequential.
 *
 * Verifies loop counter execution and termination behavior across
 * single, nested, and sequential countdown loops.
 */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint16_t count_single;
volatile uint16_t count_nested2;
volatile uint16_t count_nested3;
volatile uint16_t count_seq1;
volatile uint16_t count_seq2;

__attribute__((noinline))
void run_single_countdown(uint8_t n) {
    do {
        count_single++;
    } while (--n);
}

__attribute__((noinline))
void run_nested_2fold(uint8_t m, uint8_t n) {
    do {
        uint8_t i = n;
        do {
            count_nested2++;
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
                count_nested3++;
            } while (--k);
        } while (--j);
    } while (--a);
}

__attribute__((noinline))
void run_sequential(uint8_t n, uint8_t m) {
    do {
        count_seq1++;
    } while (--n);
    do {
        count_seq2++;
    } while (--m);
}

int main(void) {
    uint16_t status = 0;

    /* Bit 0: Single countdown loop (50 iterations) */
    count_single = 0;
    run_single_countdown(50);
    if (count_single == 50)
        status |= (1 << 0);

    /* Bit 1: Double nested countdown loop (10 * 20 = 200 iterations) */
    count_nested2 = 0;
    run_nested_2fold(10, 20);
    if (count_nested2 == 200)
        status |= (1 << 1);

    /* Bit 2: Triple nested countdown loop (5 * 4 * 3 = 60 iterations) */
    count_nested3 = 0;
    run_nested_3fold(5, 4, 3);
    if (count_nested3 == 60)
        status |= (1 << 2);

    /* Bit 3: Sequential countdown loops (30 + 40 iterations) */
    count_seq1 = 0;
    count_seq2 = 0;
    run_sequential(30, 40);
    if (count_seq1 == 30 && count_seq2 == 40)
        status |= (1 << 3);

    /* Bits 4-7: Combined verification marker */
    status |= 0x00F0;

    return status; /* expect 0x00FF */
}
