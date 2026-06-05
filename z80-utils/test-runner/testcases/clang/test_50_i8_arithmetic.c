/* Test 50: i8 arithmetic - add, sub, mul, div, mod, overflow, signed ops */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Basic add/sub: 100+55==155, 200-50==150 */
    {
        volatile uint8_t a = 100, b = 55, c = 200, d = 50;
        if ((uint8_t)(a + b) == 155 && (uint8_t)(c - d) == 150)
            status |= (1 << 0);
    }

    /* Bit 1: Multiply: 12*11==132, 15*17==255 */
    {
        volatile uint8_t a = 12, b = 11, c = 15, d = 17;
        if ((uint8_t)(a * b) == 132 && (uint8_t)(c * d) == 255)
            status |= (1 << 1);
    }

    /* Bit 2: Division and modulo: 200/7==28, 200%7==4 */
    {
        volatile uint8_t a = 200, b = 7;
        if (a / b == 28 && a % b == 4) status |= (1 << 2);
    }

    /* Bit 3: Unsigned overflow wrap: 255+1==0, 0-1==255 */
    {
        volatile uint8_t a = 255, b = 0;
        uint8_t r1 = a + 1;
        uint8_t r2 = b - 1;
        if (r1 == 0 && r2 == 255) status |= (1 << 3);
    }

    /* Bit 4: Signed arithmetic: (-50)+30==-20, (-10)*(-5)==50 */
    {
        volatile int8_t a = -50, b = 30, c = -10, d = -5;
        int8_t r1 = a + b;
        int8_t r2 = c * d;
        if (r1 == -20 && r2 == 50) status |= (1 << 4);
    }

    /* Bit 5: Signed comparison: -128 < -1 < 0 < 1 < 127 */
    {
        volatile int8_t a = -128, b = -1, c = 0, d = 1, e = 127;
        if (a < b && b < c && c < d && d < e) status |= (1 << 5);
    }

    /* Bit 6: Bitwise ops: 0xA5 & 0x5A == 0x00, 0xA5 | 0x5A == 0xFF, 0xA5 ^ 0xFF == 0x5A */
    {
        volatile uint8_t a = 0xA5, b = 0x5A, c = 0xFF;
        if ((a & b) == 0x00 && (a | b) == 0xFF && (a ^ c) == 0x5A)
            status |= (1 << 6);
    }

    /* Bit 7: Sum loop 1..20 in uint8_t == 210 */
    {
        volatile uint8_t sum = 0;
        for (uint8_t i = 1; i <= 20; i++) sum += i;
        if (sum == 210) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
