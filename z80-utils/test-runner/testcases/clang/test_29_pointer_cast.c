/* Test 29: Pointer casting - byte access, type reinterpretation, void* swap */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

void generic_swap(void *a, void *b, uint8_t size) {
    uint8_t *pa = (uint8_t *)a;
    uint8_t *pb = (uint8_t *)b;
    for (uint8_t i = 0; i < size; i++) {
        uint8_t t = pa[i];
        pa[i] = pb[i];
        pb[i] = t;
    }
}

int main() {
    uint16_t status = 0;

    /* Bit 0: access i16 as bytes: val=0x1234; bytes[0]==0x34 (LE), bytes[1]==0x12 */
    {
        volatile uint16_t val = 0x1234;
        uint8_t *bytes = (uint8_t *)&val;
        if (bytes[0] == 0x34 && bytes[1] == 0x12)
            status |= 1;
    }

    /* Bit 1: access i32 as bytes: val=0xDEADBEEF; verify all 4 bytes (LE: EF,BE,AD,DE) */
    {
        volatile uint32_t val = 0xDEADBEEFUL;
        uint8_t *bytes = (uint8_t *)&val;
        if (bytes[0] == 0xEF && bytes[1] == 0xBE &&
            bytes[2] == 0xAD && bytes[3] == 0xDE)
            status |= 2;
    }

    /* Bit 2: construct i16 from bytes: bytes[0]=0xCD, bytes[1]=0xAB -> 0xABCD */
    {
        uint8_t bytes[2];
        bytes[0] = 0xCD;
        bytes[1] = 0xAB;
        uint16_t *p = (uint16_t *)bytes;
        if (*p == 0xABCD) status |= 4;
    }

    /* Bit 3: void* generic swap: swap two uint16_t values via void* */
    {
        volatile uint16_t a = 0x1234;
        volatile uint16_t b = 0xABCD;
        generic_swap((void *)&a, (void *)&b, sizeof(uint16_t));
        if (a == 0xABCD && b == 0x1234) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
