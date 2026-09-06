/* Test 35: Memory operations - volatile, memcpy, memset, ring buffer */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: volatile read/write: verify not optimized away */
    {
        volatile uint16_t v = 0;
        v = 12345;
        uint16_t a = v;
        v = 54321;
        uint16_t b = v;
        if (a == 12345 && b == 54321) status |= 1;
    }

    /* Bit 1: manual memcpy: copy 8 bytes, verify all match */
    {
        volatile uint8_t src[8];
        volatile uint8_t dst[8];
        src[0] = 0x11; src[1] = 0x22; src[2] = 0x33; src[3] = 0x44;
        src[4] = 0x55; src[5] = 0x66; src[6] = 0x77; src[7] = 0x88;
        for (uint8_t i = 0; i < 8; i++)
            dst[i] = src[i];
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 8; i++)
            if (dst[i] != src[i]) ok = 0;
        if (ok) status |= 2;
    }

    /* Bit 2: manual memset + verify: set 6 bytes to 0xAA then 0x00 */
    {
        volatile uint8_t buf[6];
        uint8_t i;
        for (i = 0; i < 6; i++) buf[i] = 0xAA;
        uint8_t ok = 1;
        for (i = 0; i < 6; i++)
            if (buf[i] != 0xAA) ok = 0;
        for (i = 0; i < 6; i++) buf[i] = 0x00;
        for (i = 0; i < 6; i++)
            if (buf[i] != 0x00) ok = 0;
        if (ok) status |= 4;
    }

    /* Bit 3: ring buffer: write 6 values into 4-slot circular buffer, read last 4 */
    {
        volatile uint8_t ring[4];
        uint8_t head = 0;
        volatile uint8_t items[6];
        items[0] = 10; items[1] = 20; items[2] = 30;
        items[3] = 40; items[4] = 50; items[5] = 60;
        for (uint8_t i = 0; i < 6; i++) {
            ring[head] = items[i];
            head = (head + 1) & 3; /* mod 4 */
        }
        /* After 6 writes: ring = [50, 60, 30, 40], head=2 */
        /* Reading from head backwards: last 4 are 30,40,50,60 */
        /* ring[0]=50, ring[1]=60, ring[2]=30, ring[3]=40 */
        uint16_t rsum = 0;
        for (uint8_t i = 0; i < 4; i++) rsum += ring[i];
        /* 50+60+30+40 = 180 */
        if (rsum == 180 && ring[0] == 50 && ring[1] == 60)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
