/* Benchmark: i16 arithmetic */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Multiply and divide roundtrip: 1234*5=6170, 6170/5=1234 */
    {
        volatile uint16_t a = 1234, b = 5;
        uint16_t prod = a * b;
        uint16_t back = prod / b;
        if (prod == 6170 && back == 1234) status |= 1;
    }

    /* Bit 1: Unsigned div/mod: 50000/700=71 rem 300 */
    {
        volatile uint16_t a = 50000u, b = 700;
        uint16_t q = a / b;
        uint16_t r = a % b;
        if (q == 71 && r == 300) status |= 2;
    }

    /* Bit 2: Signed div/mod: (-3300)/11=-300, (-3300)%11=0 */
    {
        volatile int16_t a = -3300, b = 11;
        int16_t q = a / b;
        int16_t r = a % b;
        if (q == -300 && r == 0) status |= 4;
    }

    /* Bit 3: Shift and rotate: SHL 1 (ADD HL,HL), SHR, rotate left 4 */
    {
        volatile uint16_t a = 0x1234;
        uint16_t shl1 = a << 1;    /* 0x2468 */
        uint16_t shr4 = a >> 4;    /* 0x0123 */
        uint16_t rotl = (uint16_t)((a << 4) | (a >> 12)); /* 0x2341 */
        if (shl1 == 0x2468 && shr4 == 0x0123 && rotl == 0x2341) status |= 8;
    }

    /* Bit 4: Bitwise AND/OR/XOR */
    {
        volatile uint16_t a = 0xFF00, b = 0x0FF0;
        uint16_t x = a ^ b;  /* 0xF0F0 */
        uint16_t y = a & b;  /* 0x0F00 */
        uint16_t z = a | b;  /* 0xFFF0 */
        if (x == 0xF0F0 && y == 0x0F00 && z == 0xFFF0) status |= 0x10;
    }

    /* Bit 5: Add/sub chain: 10000+5000-3000+2000-4000 = 10000 */
    {
        volatile uint16_t a = 10000, b = 5000, c = 3000, d = 2000, e = 4000;
        uint16_t r = a + b;
        r = r - c;
        r = r + d;
        r = r - e;
        if (r == 10000) status |= 0x20;
    }

    /* Bit 6: Fibonacci loop: fib(10) = 55 */
    {
        volatile uint16_t a = 0, b = 1;
        uint8_t i;
        for (i = 0; i < 10; i++) {
            uint16_t t = a + b;
            a = b;
            b = t;
        }
        if (a == 55) status |= 0x40;
    }

    /* Bit 7: Unsigned overflow: 65530+10=4, 5-10=65531 */
    {
        volatile uint16_t a = 65530u, b = 10, c = 5;
        uint16_t sum = a + b;
        uint16_t diff = c - b;
        if (sum == 4 && diff == 65531u) status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
