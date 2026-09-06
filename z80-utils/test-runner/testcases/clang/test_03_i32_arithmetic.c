/* Test 03: i32 arithmetic - add, sub, multiply, factorial, div/mod, carry */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i32 add: 100000+200000==300000; i32 sub: 500000-200001==299999 */
    {
        volatile uint32_t a = 100000UL, b = 200000UL;
        volatile uint32_t c = 500000UL, d = 200001UL;
        if (a + b == 300000UL && c - d == 299999UL)
            status |= (1 << 0);
    }

    /* Bit 1: i32 square: 12345*12345==152399025 */
    {
        volatile uint32_t a = 12345UL;
        uint32_t r = a * a;
        if (r == 152399025UL) status |= (1 << 1);
    }

    /* Bit 2: signed i32 multiply: (-10000)*300==-3000000 */
    {
        volatile int32_t a = -10000L;
        volatile int32_t b = 300L;
        int32_t r = a * b;
        if (r == -3000000L) status |= (1 << 2);
    }

    /* Bit 3: i32 factorial loop: 1*2*3*...*8 == 40320 */
    {
        uint32_t r = 1;
        volatile uint8_t n = 8;
        for (uint8_t i = 1; i <= n; i++)
            r *= (uint32_t)i;
        if (r == 40320UL) status |= (1 << 3);
    }

    /* Bit 4: i32 division/modulo: 1000000/7==142857, 1000000%7==1 */
    {
        volatile uint32_t a = 1000000UL, b = 7;
        if (a / b == 142857UL && a % b == 1) status |= (1 << 4);
    }

    /* Bit 5: i32 carry propagation: 0x0000FFFF + 0x0000FFFF == 0x0001FFFE */
    {
        volatile uint32_t a = 0x0000FFFFUL, b = 0x0000FFFFUL;
        if (a + b == 0x0001FFFEUL) status |= (1 << 5);
    }

    /* Bit 6: i32 borrow propagation: 0x00010000 - 1 == 0x0000FFFF */
    {
        volatile uint32_t a = 0x00010000UL;
        if (a - 1 == 0x0000FFFFUL) status |= (1 << 6);
    }

    /* Bit 7: Signed i32 division: -1000000/300==-3333, -1000000%300==-100 */
    {
        volatile int32_t a = -1000000L, b = 300;
        if (a / b == -3333L && a % b == -100L) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
