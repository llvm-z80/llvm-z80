/* Test 14: rotate operations - 8-bit and 16-bit rotations */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

uint8_t rotl8(uint8_t v, uint8_t n) {
    n &= 7;
    return (v << n) | (v >> (8 - n));
}

uint8_t rotr8(uint8_t v, uint8_t n) {
    n &= 7;
    return (v >> n) | (v << (8 - n));
}

uint16_t rotl16(uint16_t v, uint8_t n) {
    n &= 15;
    return (v << n) | (v >> (16 - n));
}

int main() {
    uint16_t status = 0;

    /* Bit 0: 8-bit rotate left: rotl8(0x81, 1)==0x03, rotl8(0x81, 3)==0x0C */
    {
        volatile uint8_t v = 0x81;
        if (rotl8(v, 1) == 0x03 && rotl8(v, 3) == 0x0C)
            status |= (1 << 0);
    }

    /* Bit 1: 8-bit rotate right: rotr8(0x81, 1)==0xC0, rotr8(0x81, 2)==0x60 */
    {
        volatile uint8_t v = 0x81;
        if (rotr8(v, 1) == 0xC0 && rotr8(v, 2) == 0x60)
            status |= (1 << 1);
    }

    /* Bit 2: 16-bit rotate left: rotl16(0x8001, 1)==0x0003,
       rotl16(0xA5A5, 8)==0xA5A5 */
    {
        volatile uint16_t a = 0x8001, b = 0xA5A5;
        if (rotl16(a, 1) == 0x0003 && rotl16(b, 8) == 0xA5A5)
            status |= (1 << 2);
    }

    /* Bit 3: rotate by 0 is identity:
       rotl8(0x42,0)==0x42, rotr8(0x42,0)==0x42, rotl16(0x1234,0)==0x1234 */
    {
        volatile uint8_t v8 = 0x42;
        volatile uint16_t v16 = 0x1234;
        if (rotl8(v8, 0) == 0x42 && rotr8(v8, 0) == 0x42 &&
            rotl16(v16, 0) == 0x1234)
            status |= (1 << 3);
    }

    /* Bit 4: Full rotation is identity: rotl8(x,8)==x, rotl16(x,16)==x */
    {
        volatile uint8_t v8 = 0xA5;
        volatile uint16_t v16 = 0xBEEF;
        if (rotl8(v8, 8) == 0xA5 && rotl16(v16, 16) == 0xBEEF)
            status |= (1 << 4);
    }

    /* Bit 5: rotl and rotr are inverses: rotr8(rotl8(x,n),n)==x */
    {
        volatile uint8_t v = 0x73;
        if (rotr8(rotl8(v, 3), 3) == 0x73 && rotr8(rotl8(v, 5), 5) == 0x73)
            status |= (1 << 5);
    }

    /* Bit 6: 16-bit rotate by 4: rotl16(0x1234,4)==0x2341 */
    {
        volatile uint16_t v = 0x1234;
        if (rotl16(v, 4) == 0x2341) status |= (1 << 6);
    }

    /* Bit 7: 8-bit rotate by 4: rotl8(0xAB,4)==0xBA, rotr8(0xAB,4)==0xBA */
    {
        volatile uint8_t v = 0xAB;
        if (rotl8(v, 4) == 0xBA && rotr8(v, 4) == 0xBA)
            status |= (1 << 7);
    }

    return status; /* expect 0x00FF */
}
