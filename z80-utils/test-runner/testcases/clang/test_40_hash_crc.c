/* Test 40: Hash and CRC - CRC-16, CRC-32, djb2, xorshift16 */
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

uint16_t hash16(const uint8_t *data, uint8_t len) {
    uint16_t h = 5381;
    for (uint8_t i = 0; i < len; i++)
        h = h * 33 + data[i];
    return h;
}

uint16_t xorshift16(uint16_t state) {
    state ^= state << 7;
    state ^= state >> 9;
    state ^= state << 8;
    return state;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: CRC-16 of "AB" (2 bytes 0x41,0x42) = 0x4B74 (verified in test_13) */
    {
        uint8_t data[2];
        data[0] = 0x41; data[1] = 0x42;
        uint16_t r = crc16(data, 2);
        if (r == 0x4B74) status |= 1;
    }

    /* Bit 1: CRC-32 of single byte 'A' (0x41) = 0xD3D99E8B (verified in test_69) */
    {
        uint8_t data[1];
        data[0] = 0x41;
        uint32_t r = crc32(data, 1);
        if (r == 0xD3D99E8BUL) status |= 2;
    }

    /* Bit 2: djb2 hash: hash16 of "hello" = 0x3099 */
    {
        uint8_t data[5];
        data[0] = 'h'; data[1] = 'e'; data[2] = 'l';
        data[3] = 'l'; data[4] = 'o';
        uint16_t h = hash16(data, 5);
        if (h == 0x3099) status |= 4;
    }

    /* Bit 3: xorshift16 PRNG: seed=1234, 3 iterations, verify 3rd value = 0xF03E */
    {
        volatile uint16_t state = 1234;
        state = xorshift16(state); /* 0x89E4 */
        state = xorshift16(state); /* 0xA2D9 */
        state = xorshift16(state); /* 0xF03E */
        if (state == 0xF03E) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
