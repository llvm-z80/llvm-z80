// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x01FF %t

/* Signed and unsigned 32-bit products of 16-bit operands, lowered to
 * __mulhisi3 and __umulhisi3. Every sign combination is checked, along with
 * the ends of both ranges. */

__attribute__((noinline)) static long smul(int a, int b) {
    return (long)a * b;
}

__attribute__((noinline)) static unsigned long umul(unsigned a, unsigned b) {
    return (unsigned long)a * b;
}

int main() {
    int status = 0;

    if (smul(-300, 200) == -60000L)
        status |= 1;
    if (smul(300, -200) == -60000L)
        status |= 2;
    if (smul(-300, -200) == 60000L)
        status |= 4;
    if (smul(-32768, -32768) == 0x40000000L)
        status |= 8;
    if (smul(-32768, 32767) == -1073709056L)
        status |= 16;
    if (umul(65535u, 65535u) == 0xFFFE0001UL)
        status |= 32;
    if (umul(40000u, 3u) == 120000UL)
        status |= 64;
    if (smul(-1, -1) == 1L)
        status |= 128;
    if (smul(300, 200) == 60000L)
        status |= 256;

    return status;
}
