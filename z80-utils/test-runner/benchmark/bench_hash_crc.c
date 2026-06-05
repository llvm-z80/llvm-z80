/* Benchmark: Hash and CRC */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

uint16_t crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

uint32_t crc32(const uint8_t *data, uint16_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320UL : 0);
    }
    return ~crc;
}

uint16_t djb2_16(const uint8_t *data, uint8_t len) {
    uint16_t h = 5381;
    for (uint8_t i = 0; i < len; i++)
        h = h * 33 + data[i];
    return h;
}

uint16_t fnv1a_16(const uint8_t *data, uint8_t len) {
    uint16_t h = 0x811C; /* FNV offset basis truncated to 16-bit */
    for (uint8_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x0193; /* FNV prime truncated to 16-bit */
    }
    return h;
}

uint16_t xorshift16(uint16_t state) {
    state ^= state << 7;
    state ^= state >> 9;
    state ^= state << 8;
    return state;
}

uint8_t simple_checksum(const uint8_t *data, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++)
        sum += data[i];
    return ~sum + 1; /* two's complement: sum + checksum = 0 */
}

int main() {
    uint16_t status = 0;

    /* Bit 0: CRC-16 of "AB" = 0x4B74 */
    {
        uint8_t data[2];
        data[0] = 0x41; data[1] = 0x42;
        uint16_t r = crc16(data, 2);
        if (r == 0x4B74) status |= 1;
    }

    /* Bit 1: CRC-32 of 'A' = 0xD3D99E8B */
    {
        uint8_t data[1];
        data[0] = 0x41;
        uint32_t r = crc32(data, 1);
        if (r == 0xD3D99E8BUL) status |= 2;
    }

    /* Bit 2: djb2 hash of "hello" = 0x3099 */
    {
        uint8_t data[5];
        data[0] = 'h'; data[1] = 'e'; data[2] = 'l';
        data[3] = 'l'; data[4] = 'o';
        uint16_t h = djb2_16(data, 5);
        if (h == 0x3099) status |= 4;
    }

    /* Bit 3: xorshift16 seed=1234, 3 iters -> 0xF03E */
    {
        volatile uint16_t state = 1234;
        state = xorshift16(state);
        state = xorshift16(state);
        state = xorshift16(state);
        if (state == 0xF03E) status |= 8;
    }

    /* Bit 4: FNV-1a hash of "test" */
    {
        uint8_t data[4];
        data[0] = 't'; data[1] = 'e'; data[2] = 's'; data[3] = 't';
        uint16_t h = fnv1a_16(data, 4);
        if (h == 0xB318) status |= 0x10;
    }

    /* Bit 5: Simple checksum: sum + checksum = 0 */
    {
        uint8_t data[4];
        data[0] = 0x12; data[1] = 0x34; data[2] = 0x56; data[3] = 0x78;
        uint8_t cs = simple_checksum(data, 4);
        /* sum = 0x12+0x34+0x56+0x78 = 0x114 (truncated 0x14), cs = ~0x14+1 = 0xEC */
        uint8_t verify = 0;
        for (uint8_t i = 0; i < 4; i++) verify += data[i];
        verify += cs;
        if (verify == 0 && cs == 0xEC) status |= 0x20;
    }

    /* Bit 6: CRC-16 of longer data "ABCDEF" */
    {
        uint8_t data[6];
        data[0] = 'A'; data[1] = 'B'; data[2] = 'C';
        data[3] = 'D'; data[4] = 'E'; data[5] = 'F';
        uint16_t r = crc16(data, 6);
        if (r == 0x9A5D) status |= 0x40;
    }

    /* Bit 7: xorshift16 period test: 10 iterations, verify not stuck */
    {
        volatile uint16_t state = 42;
        uint16_t seen_first;
        state = xorshift16(state);
        seen_first = state;
        uint8_t all_different = 1;
        for (uint8_t i = 0; i < 9; i++) {
            uint16_t prev = state;
            state = xorshift16(state);
            if (state == prev) all_different = 0;
        }
        /* After 10 total iterations from seed=42, verify state is non-trivial */
        if (all_different && state != 42 && state != 0) status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
