// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0003 %t

/* A frame slot stored, written again through a pointer and read back. The
 * read has to see the value written through the pointer. The recursive call
 * keeps each call of f in a separate frame. */
typedef unsigned short uint16_t;

uint16_t *volatile gp;

uint16_t f(uint16_t c) {
    uint16_t x;
    gp = &x;
    uint16_t *p = gp;
    x = c;
    *p = 9;
    uint16_t r = x;
    return r + (c == 100 ? f(c - 1) : 0);
}

int main() {
    uint16_t status = 0;

    if (f(5) == 9)
        status |= 1 << 0;
    if (f(100) == 18)
        status |= 1 << 1;

    return status;
}
