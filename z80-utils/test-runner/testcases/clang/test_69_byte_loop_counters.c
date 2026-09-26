/* Test 69: Loops whose counter never leaves a byte.
 *
 * A 16-bit loop variable that only takes values in 0-255, and whose uses
 * want no more than its low byte, is counted in 8 bits.  The loops below
 * store the counter, index with it, compare it against constant and variable
 * bounds, signed and unsigned, and count down; the edges are an empty loop, a
 * single pass, and a bound of 255.  A loop whose counter reaches 256 keeps
 * its width and is checked alongside. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

static uint8_t buf[256];

__attribute__((noinline)) static void fill16(uint8_t *d) {
    for (int i = 0; i < 16; i++)
        d[i] = (uint8_t)i;
}

__attribute__((noinline)) static uint16_t count_to(uint8_t n) {
    uint16_t c = 0;
    for (int i = 0; i < n; i++)
        c += 3;
    return c;
}

__attribute__((noinline)) static uint16_t count_to_signed(signed char n) {
    uint16_t c = 0;
    for (int i = 0; i < n; i++)
        c++;
    return c;
}

__attribute__((noinline)) static uint16_t count_down(uint8_t n) {
    uint16_t c = 0;
    for (int i = n; i > 0; i--)
        c += 2;
    return c;
}

__attribute__((noinline)) static void fill_to_255(uint8_t *d) {
    for (unsigned i = 0; i != 255; i++)
        d[i] = (uint8_t)(i ^ 0x5A);
}

/* The counter reaches 256 here, which does not fit a byte. */
__attribute__((noinline)) static uint16_t count_256(void) {
    uint16_t c = 0;
    for (unsigned i = 0; i < 256; i++)
        c++;
    return c;
}

int main(void) {
    uint16_t status = 0;
    uint16_t i;
    uint8_t ok;

    fill16(buf);
    ok = 1;
    for (i = 0; i < 16; i++)
        ok &= buf[i] == i;
    if (ok)
        status |= 1;

    if (count_to(0) == 0 && count_to(1) == 3 && count_to(200) == 600 &&
        count_to(255) == 765)
        status |= 2;

    if (count_to_signed(0) == 0 && count_to_signed(-5) == 0 &&
        count_to_signed(1) == 1 && count_to_signed(127) == 127)
        status |= 4;

    if (count_down(0) == 0 && count_down(1) == 2 && count_down(255) == 510)
        status |= 8;

    buf[255] = 0xEE;
    fill_to_255(buf);
    ok = buf[255] == 0xEE;
    for (i = 0; i < 255; i++)
        ok &= buf[i] == (uint8_t)(i ^ 0x5A);
    if (ok)
        status |= 16;

    if (count_256() == 256)
        status |= 32;

    return status; /* expect 0x003F */
}
