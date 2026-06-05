/* Test 22: Recursion - factorial, mutual recursion, Ackermann, Hofstadter */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

uint16_t factorial(uint16_t n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

/* Mutual recursion: is_even / is_odd */
uint8_t is_odd(uint16_t n);

uint8_t is_even(uint16_t n) {
    if (n == 0) return 1;
    return is_odd(n - 1);
}

uint8_t is_odd(uint16_t n) {
    if (n == 0) return 0;
    return is_even(n - 1);
}

/* Ackermann function */
uint16_t ackermann(uint16_t m, uint16_t n) {
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

/* Hofstadter Female and Male sequences */
uint16_t hof_male(uint16_t n);

uint16_t hof_female(uint16_t n) {
    if (n == 0) return 1;
    return n - hof_male(hof_female(n - 1));
}

uint16_t hof_male(uint16_t n) {
    if (n == 0) return 0;
    return n - hof_female(hof_male(n - 1));
}

int main() {
    uint16_t status = 0;

    /* Bit 0: recursive factorial - fact(6) == 720 */
    {
        volatile uint16_t n = 6;
        if (factorial(n) == 720)
            status |= 1;
    }

    /* Bit 1: mutual recursion is_even/is_odd */
    {
        volatile uint16_t v10 = 10, v7 = 7, v3 = 3;
        if (is_even(v10) == 1 && is_odd(v7) == 1 && is_even(v3) == 0)
            status |= 2;
    }

    /* Bit 2: Ackermann A(2,3)==9, A(3,2)==29 */
    {
        volatile uint16_t m1 = 2, n1 = 3;
        volatile uint16_t m2 = 3, n2 = 2;
        if (ackermann(m1, n1) == 9 && ackermann(m2, n2) == 29)
            status |= 4;
    }

    /* Bit 3: Hofstadter F(5)==3, M(5)==3 */
    /* F: 1, 1, 2, 2, 3, 3 (n=0..5) */
    /* M: 0, 0, 1, 2, 2, 3 (n=0..5) */
    {
        volatile uint16_t n = 5;
        if (hof_female(n) == 3 && hof_male(n) == 3)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
