/* Test 17: Ternary/select - clamp, abs, sign, conditional accumulation, chained */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int16_t clamp16(int16_t val, int16_t lo, int16_t hi) {
    return val < lo ? lo : (val > hi ? hi : val);
}

int16_t abs16(int16_t x) {
    return x < 0 ? -x : x;
}

int16_t sign16(int16_t x) {
    return x > 0 ? 1 : (x < 0 ? -1 : 0);
}

/* Map value 0-3 to "string length": 0->3("foo"), 1->5("hello"), 2->4("test"), 3->4("four"), else->7("unknown") */
uint8_t name_len(uint8_t val) {
    return val == 0 ? 3
         : val == 1 ? 5
         : val == 2 ? 4
         : val == 3 ? 4
         : 7;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: basic ternary clamp */
    {
        volatile int16_t v1 = 50;
        volatile int16_t v2 = -5;
        volatile int16_t v3 = 200;
        uint8_t ok = 1;
        if (clamp16(v1, 0, 100) != 50)  ok = 0;  /* in range */
        if (clamp16(v2, 0, 100) != 0)   ok = 0;  /* below min */
        if (clamp16(v3, 0, 100) != 100) ok = 0;  /* above max */
        if (ok) status |= 1;
    }

    /* Bit 1: abs and sign via ternary */
    {
        volatile int16_t a = -42;
        volatile int16_t b = 42;
        volatile int16_t c = 5;
        volatile int16_t d = -5;
        volatile int16_t e = 0;
        uint8_t ok = 1;
        if (abs16(a) != 42) ok = 0;
        if (abs16(b) != 42) ok = 0;
        if (sign16(c) != 1)  ok = 0;
        if (sign16(d) != -1) ok = 0;
        if (sign16(e) != 0)  ok = 0;
        if (ok) status |= 2;
    }

    /* Bit 2: conditional accumulation - sum positive values only */
    {
        volatile int16_t arr[8];
        arr[0] = 3; arr[1] = -1; arr[2] = 4; arr[3] = -1;
        arr[4] = 5; arr[5] = -9; arr[6] = 2; arr[7] = 6;
        /* positive: 3 + 4 + 5 + 2 + 6 = 20 */

        int16_t sum = 0;
        for (uint8_t i = 0; i < 8; i++) {
            sum += arr[i] > 0 ? arr[i] : 0;
        }
        if (sum == 20)
            status |= 4;
    }

    /* Bit 3: chained ternary - name_len mapping */
    {
        volatile uint8_t v0 = 0, v1 = 1, v2 = 2, v3 = 3;
        uint8_t ok = 1;
        if (name_len(v0) != 3) ok = 0;
        if (name_len(v1) != 5) ok = 0;
        if (name_len(v2) != 4) ok = 0;
        if (name_len(v3) != 4) ok = 0;
        if (ok) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
