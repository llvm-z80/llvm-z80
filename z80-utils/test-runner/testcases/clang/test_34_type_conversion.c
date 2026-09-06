/* Test 34: Type conversions - sign extension, zero extension, truncation */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: sign extension: (int16_t)(int8_t)0xFE == -2; (int32_t)(int16_t)(-1)==-1 */
    {
        volatile uint8_t raw = 0xFE;
        int16_t extended = (int16_t)(int8_t)raw;
        /* 0xFE as signed i8 = -2; sign-extended to i16 = -2 = 0xFFFE */
        volatile int16_t neg1 = -1;
        int32_t wide = (int32_t)neg1;
        if (extended == -2 && wide == -1L) status |= 1;
    }

    /* Bit 1: zero extension: (uint16_t)(uint8_t)0xFE == 254; (uint32_t)(uint16_t)0xFFFE==0x0000FFFE */
    {
        volatile uint8_t u8val = 0xFE;
        uint16_t u16val = (uint16_t)u8val;
        volatile uint16_t u16src = 0xFFFE;
        uint32_t u32val = (uint32_t)u16src;
        if (u16val == 254 && u32val == 0x0000FFFEUL) status |= 2;
    }

    /* Bit 2: truncation: (uint8_t)(uint16_t)0x1234==0x34; (uint16_t)(uint32_t)0xABCD1234==0x1234 */
    {
        volatile uint16_t w = 0x1234;
        uint8_t lo = (uint8_t)w;
        volatile uint32_t dw = 0xABCD1234UL;
        uint16_t lo16 = (uint16_t)dw;
        if (lo == 0x34 && lo16 == 0x1234) status |= 4;
    }

    /* Bit 3: signed<->unsigned: (uint16_t)(int16_t)-1==0xFFFF; (int16_t)(uint16_t)0x8000==-32768 */
    {
        volatile int16_t neg1 = -1;
        uint16_t as_unsigned = (uint16_t)neg1;
        volatile uint16_t val = 0x8000;
        int16_t as_signed = (int16_t)val;
        if (as_unsigned == 0xFFFF && as_signed == -32768) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
