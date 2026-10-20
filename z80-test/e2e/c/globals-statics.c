// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x000F %t

/* Initialized globals, a static local counter, a const table and an LCG. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint16_t global_var = 42;

const uint16_t squares[5] = {0, 1, 4, 9, 16};

uint16_t get_count(void) {
    static uint16_t count = 0;
    return ++count;
}

uint16_t lcg_next(void) {
    static uint16_t state = 1;
    state = state * 31421 + 6927;
    return state;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: read and write an initialized global */
    {
        uint16_t initial = global_var;
        global_var = 100;
        uint16_t modified = global_var;
        if (initial == 42 && modified == 100) status |= 1;
    }

    /* Bit 1: static counter: called 5 times -> returns 5 */
    {
        uint16_t r1 = get_count(); /* 1 */
        uint16_t r2 = get_count(); /* 2 */
        uint16_t r3 = get_count(); /* 3 */
        uint16_t r4 = get_count(); /* 4 */
        uint16_t r5 = get_count(); /* 5 */
        if (r5 == 5) status |= 2;
    }

    /* Bit 2: const lookup table: squares[3]==9 */
    {
        if (squares[0] == 0 && squares[3] == 9 && squares[4] == 16)
            status |= 4;
    }

    /* Bit 3: third LCG value from seed 1 */
    {
        uint16_t r1 = lcg_next();
        uint16_t r2 = lcg_next();
        uint16_t r3 = lcg_next();
        if (r3 == 55630) status |= 8;
    }

    return status;
}
