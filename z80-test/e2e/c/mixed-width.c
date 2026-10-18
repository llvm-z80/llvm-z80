// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x00FF %t

/* Widening, narrowing and arithmetic across integer widths. */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i8*i8 -> i16 widening multiply: 200*200==40000 */
    {
        volatile uint8_t a = 200, b = 200;
        uint16_t r = (uint16_t)a * (uint16_t)b;
        if (r == 40000) status |= (1 << 0);
    }

    /* Bit 1: i16*i16 -> i32 widening multiply: 30000*30000==900000000 */
    {
        volatile uint16_t a = 30000, b = 30000;
        uint32_t r = (uint32_t)a * (uint32_t)b;
        if (r == 900000000UL) status |= (1 << 1);
    }

    /* Bit 2: i32 -> i16 -> i8 truncation: 0x12345678 -> 0x5678 -> 0x78 */
    {
        volatile uint32_t a = 0x12345678UL;
        uint16_t mid = (uint16_t)a;
        uint8_t lo = (uint8_t)mid;
        if (mid == 0x5678 && lo == 0x78) status |= (1 << 2);
    }

    /* Bit 3: signed widening: (int8_t)-1 -> (int16_t)==-1, -> (int32_t)==-1 */
    {
        volatile int8_t a = -1;
        int16_t w16 = a;
        int32_t w32 = a;
        if (w16 == -1 && w32 == -1L) status |= (1 << 3);
    }

    /* Bit 4: i8 plus i16: (uint8_t)200 + (uint16_t)60000 == 60200 */
    {
        volatile uint8_t a = 200;
        volatile uint16_t b = 60000;
        uint16_t r = a + b;
        if (r == 60200) status |= (1 << 4);
    }

    /* Bit 5: signed narrowing: (int16_t)300 -> (int8_t) == 0x2C == 44 */
    {
        volatile int16_t a = 300;
        int8_t narrow = (int8_t)a;
        if (narrow == 44) status |= (1 << 5);
    }

    /* Bit 6: i8 accumulation into i16: sum 50 values of 100 = 5000 */
    {
        volatile uint8_t val = 100;
        uint16_t sum = 0;
        for (uint8_t i = 0; i < 50; i++) sum += val;
        if (sum == 5000) status |= (1 << 6);
    }

    /* Bit 7: 0x80 zero-extends to 128 and sign-extends to -128 */
    {
        volatile uint8_t u = 0x80;
        volatile int8_t s = (int8_t)0x80;
        uint16_t zu = u;    /* zero-extend: 128 */
        int16_t si = s;     /* sign-extend: -128 */
        if (zu == 128 && si == -128) status |= (1 << 7);
    }

    return status;
}
