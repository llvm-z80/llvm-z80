/* Benchmark: high register pressure with many simultaneous stack reloads.
 * Each test loads multiple volatile 16-bit stack variables and combines them
 * with 16-bit addition/subtraction, forcing many RELOAD_GR16 + ADD_HL_rr.
 *
 * Expected result: 0x000F (4 bits set)
 */
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

int main() {
    uint16_t status = 0;

    /* Test 1: sum of 8 volatile variables = a+b+c+d+e+f+g+h
     * Each variable is on the stack; adding them requires reloading into
     * a register pair. With 8 additions, register pressure is high. */
    {
        volatile uint16_t a = 10, b = 20, c = 30, d = 40;
        volatile uint16_t e = 50, f = 60, g = 70, h = 80;
        uint16_t sum = a + b + c + d + e + f + g + h;
        if (sum == 360) status |= 1;
    }

    /* Test 2: interleaved add/sub with 6 variables
     * result = a + b - c + d - e + f */
    {
        volatile uint16_t a = 100, b = 200, c = 50;
        volatile uint16_t d = 300, e = 150, f = 100;
        uint16_t result = a + b - c + d - e + f;
        if (result == 500) status |= 2;
    }

    /* Test 3: multiple cross-references — each result uses different pairs
     * r1 = a+b, r2 = c+d, r3 = e+f, final = r1+r2+r3+a+c+e
     * Many values alive simultaneously */
    {
        volatile uint16_t a = 5, b = 10, c = 15, d = 20, e = 25, f = 30;
        uint16_t r1 = a + b;    /* 15 */
        uint16_t r2 = c + d;    /* 35 */
        uint16_t r3 = e + f;    /* 55 */
        uint16_t final_val = r1 + r2 + r3 + a + c + e;
        /* 15 + 35 + 55 + 5 + 15 + 25 = 150 */
        if (final_val == 150) status |= 4;
    }

    /* Test 4: deeply nested expression with many reloads
     * result = ((a+b)+(c+d)) + ((e+f)+(g+h)) + ((a+c)+(e+g))
     * This creates a tree of additions where many stack values must be
     * reloaded into BC/DE simultaneously. */
    {
        volatile uint16_t a = 1, b = 2, c = 3, d = 4;
        volatile uint16_t e = 5, f = 6, g = 7, h = 8;
        uint16_t ab = a + b;    /* 3 */
        uint16_t cd = c + d;    /* 7 */
        uint16_t ef = e + f;    /* 11 */
        uint16_t gh = g + h;    /* 15 */
        uint16_t ac = a + c;    /* 4 */
        uint16_t eg = e + g;    /* 12 */
        uint16_t result = ab + cd + ef + gh + ac + eg;
        /* 3+7+11+15+4+12 = 52 */
        if (result == 52) status |= 8;
    }

    return status; /* expect 0x000F */
}
