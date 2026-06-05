/* Test 01: i16 arithmetic - Fibonacci, factorial, compound ops, overflow, div/mod */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Fibonacci fib(10) = 55 via iterative loop */
    {
        volatile uint16_t a = 0, b = 1;
        for (uint16_t i = 0; i < 10; i++) {
            uint16_t t = a + b;
            a = b;
            b = t;
        }
        /* After 10 iterations: a=55, b=89 */
        if (a == 55) status |= (1 << 0);
    }

    /* Bit 1: Factorial fact(8) = 40320 (use i32 result from i16 loop) */
    {
        volatile uint32_t fact = 1;
        for (uint16_t i = 2; i <= 8; i++) {
            fact *= (uint32_t)i;
        }
        if (fact == 40320UL) status |= (1 << 1);
    }

    /* Bit 2: Compound assignment: a=10; a+=5; a*=3; a-=5; a/=2; a%=7 */
    {
        volatile uint16_t a = 10;
        a += 5;   /* 15 */
        a *= 3;   /* 45 */
        a -= 5;   /* 40 */
        a /= 2;   /* 20 */
        a %= 7;   /* 20 % 7 = 6 */
        if (a == 6) status |= (1 << 2);
    }

    /* Bit 3: Pre/post increment: a=5; b=a++; c=++a; verify b==5, a==7, c==7 */
    {
        volatile uint16_t a = 5;
        uint16_t b = a++;  /* b=5, a=6 */
        uint16_t c = ++a;  /* a=7, c=7 */
        if (b == 5 && a == 7 && c == 7) status |= (1 << 3);
    }

    /* Bit 4: Sum of arithmetic series 1+2+...+100 = 5050 */
    {
        volatile uint16_t sum = 0;
        for (uint16_t i = 1; i <= 100; i++) sum += i;
        if (sum == 5050) status |= (1 << 4);
    }

    /* Bit 5: Unsigned overflow wrap: 65535+1==0, 0-1==65535 */
    {
        volatile uint16_t a = 65535, b = 0;
        uint16_t r1 = a + 1;
        uint16_t r2 = b - 1;
        if (r1 == 0 && r2 == 65535) status |= (1 << 5);
    }

    /* Bit 6: Division and modulo combined: 1000/7==142, 1000%7==6 */
    {
        volatile uint16_t a = 1000, b = 7;
        if (a / b == 142 && a % b == 6) status |= (1 << 6);
    }

    /* Bit 7: Chained multiply: 3*5*7*11 = 1155 */
    {
        volatile uint16_t a = 3, b = 5, c = 7, d = 11;
        uint16_t r = a * b * c * d;
        if (r == 1155) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
