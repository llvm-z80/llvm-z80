/* Test 58: fixed-point arithmetic using _Accum type */
/* EXTRA-FLAGS: -ffixed-point */
/* expect 0x003F */

int main() {
    int status = 0;

    /* Bit 0: basic multiply 2.5 * 2.0 = 5.0 */
    {
        _Accum a = 2.5k;
        _Accum b = 2.0k;
        _Accum c = a * b;
        if (c == 5.0k) status |= (1 << 0);
    }

    /* Bit 1: basic add 1.5 + 2.25 = 3.75 */
    {
        _Accum a = 1.5k;
        _Accum b = 2.25k;
        _Accum c = a + b;
        if (c == 3.75k) status |= (1 << 1);
    }

    /* Bit 2: subtract 5.0 - 1.5 = 3.5 */
    {
        _Accum a = 5.0k;
        _Accum b = 1.5k;
        _Accum c = a - b;
        if (c == 3.5k) status |= (1 << 2);
    }

    /* Bit 3: divide 6.0 / 2.0 = 3.0 */
    {
        _Accum a = 6.0k;
        _Accum b = 2.0k;
        _Accum c = a / b;
        if (c == 3.0k) status |= (1 << 3);
    }

    /* Bit 4: negative multiply (-1.5) * 2.0 = -3.0 */
    {
        _Accum a = -1.5k;
        _Accum b = 2.0k;
        _Accum c = a * b;
        if (c == -3.0k) status |= (1 << 4);
    }

    /* Bit 5: conversion _Accum to int: (int)3.75k = 3 */
    {
        _Accum a = 3.75k;
        int v = (int)a;
        if (v == 3) status |= (1 << 5);
    }

    return status;
}
