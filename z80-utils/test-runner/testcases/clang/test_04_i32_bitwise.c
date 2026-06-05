/* Test 04: i32 bitwise - AND/OR/XOR, shifts, byte extraction, NOT, assembly */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: i32 AND/OR/XOR:
       0xAABBCCDD & 0xFF00FF00 == 0xAA00CC00;
       result | 0x00110022 == 0xAA11CC22;
       val ^ val == 0 */
    {
        volatile uint32_t a = 0xAABBCCDDUL;
        volatile uint32_t b = 0xFF00FF00UL;
        uint32_t r_and = a & b;
        if (r_and == 0xAA00CC00UL) {
            uint32_t r_or = r_and | 0x00110022UL;
            if (r_or == 0xAA11CC22UL) {
                uint32_t r_xor = a ^ a;
                if (r_xor == 0) status |= (1 << 0);
            }
        }
    }

    /* Bit 1: i32 left shift: 1<<16==65536, 0x12345678<<8==0x34567800 */
    {
        volatile uint32_t a = 1;
        volatile uint32_t b = 0x12345678UL;
        if ((a << 16) == 65536UL && (b << 8) == 0x34567800UL)
            status |= (1 << 1);
    }

    /* Bit 2: i32 right shift: 0x12345678>>16==0x1234, 0x12345678>>8==0x00123456 */
    {
        volatile uint32_t a = 0x12345678UL;
        if ((a >> 16) == 0x1234UL && (a >> 8) == 0x00123456UL)
            status |= (1 << 2);
    }

    /* Bit 3: byte extraction:
       val=0xDEADBEEF; (val>>24)&0xFF==0xDE, (val>>16)&0xFF==0xAD,
       (val>>8)&0xFF==0xBE, val&0xFF==0xEF */
    {
        volatile uint32_t val = 0xDEADBEEFUL;
        uint8_t b3 = (uint8_t)((val >> 24) & 0xFF);
        uint8_t b2 = (uint8_t)((val >> 16) & 0xFF);
        uint8_t b1 = (uint8_t)((val >> 8) & 0xFF);
        uint8_t b0 = (uint8_t)(val & 0xFF);
        if (b3 == 0xDE && b2 == 0xAD && b1 == 0xBE && b0 == 0xEF)
            status |= (1 << 3);
    }

    /* Bit 4: NOT (one's complement): ~0x12345678 == 0xEDCBA987 */
    {
        volatile uint32_t a = 0x12345678UL;
        if (~a == 0xEDCBA987UL) status |= (1 << 4);
    }

    /* Bit 5: Byte assembly from 4 bytes: (0xAA<<24)|(0xBB<<16)|(0xCC<<8)|0xDD */
    {
        volatile uint8_t b3 = 0xAA, b2 = 0xBB, b1 = 0xCC, b0 = 0xDD;
        uint32_t assembled = ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) |
                             ((uint32_t)b1 << 8) | b0;
        if (assembled == 0xAABBCCDDUL) status |= (1 << 5);
    }

    /* Bit 6: Bit counting via shift loop: count bits set in 0xA5A5A5A5 == 16 */
    {
        volatile uint32_t val = 0xA5A5A5A5UL;
        uint8_t count = 0;
        uint32_t v = val;
        while (v) { count += (v & 1); v >>= 1; }
        if (count == 16) status |= (1 << 6);
    }

    /* Bit 7: Swap halves: (val<<16)|(val>>16) of 0x12345678 == 0x56781234 */
    {
        volatile uint32_t val = 0x12345678UL;
        uint32_t swapped = (val << 16) | (val >> 16);
        if (swapped == 0x56781234UL) status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
