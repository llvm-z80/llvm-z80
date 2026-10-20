// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x00FF %t

/* Signed 16-bit quotient and remainder of the same operands, fused into a
 * single __divmodhi4 call, for every sign combination, -32768, a divisor of
 * 256 or more and an exact division. */

__attribute__((noinline)) static void divrem(int a, int b, int *q, int *r) {
    *q = a / b;
    *r = a % b;
}

__attribute__((noinline)) static int check(int a, int b, int q, int r) {
    int gq, gr;
    divrem(a, b, &gq, &gr);
    return gq == q && gr == r;
}

int main() {
    int status = 0;

    if (check(-7, 2, -3, -1))
        status |= 1;
    if (check(7, -2, -3, 1))
        status |= 2;
    if (check(-7, -2, 3, -1))
        status |= 4;
    if (check(7, 2, 3, 1))
        status |= 8;
    if (check(-32768, 7, -4681, -1))
        status |= 16;
    if (check(-1000, 300, -3, -100))
        status |= 32;
    if (check(1000, -300, -3, 100))
        status |= 64;
    if (check(-6, 3, -2, 0))
        status |= 128;

    return status;
}
