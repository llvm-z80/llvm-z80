/* Test 36: Stack pressure - large arrays, many locals, recursive chain */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

uint16_t chain_a(uint16_t x);
uint16_t chain_b(uint16_t x);
uint16_t chain_c(uint16_t x);

uint16_t chain_a(uint16_t x) {
    if (x == 0) return 0;
    return x + chain_b(x - 1);
}
uint16_t chain_b(uint16_t x) {
    if (x == 0) return 0;
    return x + chain_c(x - 1);
}
uint16_t chain_c(uint16_t x) {
    if (x == 0) return 0;
    return x + chain_a(x - 1);
}

int main() {
    uint16_t status = 0;

    /* Bit 0: large local array: uint8_t buf[200]; fill with i, checksum mod 256 */
    {
        volatile uint8_t buf[200];
        uint8_t i;
        for (i = 0; i < 200; i++)
            buf[i] = i;
        uint8_t cksum = 0;
        for (i = 0; i < 200; i++)
            cksum += buf[i];
        /* sum 0..199 = 19900; 19900 mod 256 = 19900 - 77*256 = 19900-19712 = 188 */
        if (cksum == 188) status |= 1;
    }

    /* Bit 1: many small locals: 8 volatile int16_t vars, compute expression using all */
    {
        volatile uint16_t a = 10, b = 20, c = 30, d = 40;
        volatile uint16_t e = 50, f = 60, g = 70, h = 80;
        uint16_t result = a + b + c + d + e + f + g + h;
        /* 10+20+30+40+50+60+70+80 = 360 */
        if (result == 360) status |= 2;
    }

    /* Bit 2: recursive chain: a calls b calls c calls a (3 deep) */
    {
        /* chain_a(6) = 6+chain_b(5) = 6+5+chain_c(4) = 6+5+4+chain_a(3)
           = 6+5+4+3+chain_b(2) = 6+5+4+3+2+chain_c(1) = 6+5+4+3+2+1+chain_a(0)
           = 6+5+4+3+2+1+0 = 21 */
        if (chain_a(6) == 21) status |= 4;
    }

    /* Bit 3: array of 64 bytes: fill, sum, verify == 64*63/2 % 65536 */
    {
        volatile uint8_t arr[64];
        uint8_t i;
        for (i = 0; i < 64; i++)
            arr[i] = i;
        uint16_t sum = 0;
        for (i = 0; i < 64; i++)
            sum += arr[i];
        /* sum 0..63 = 64*63/2 = 2016 */
        if (sum == 2016) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
