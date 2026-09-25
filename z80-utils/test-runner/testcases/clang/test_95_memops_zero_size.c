/* expect: 0x0007 */
/* Test: variable-size memcpy / memmove / memset with run-time
 * size==0 must not trash memory.
 *
 * Z80 LDIR / LDDR test BC AFTER decrement: with BC=0 they run 65536
 * iterations.  Unguarded variable-size block operations therefore
 * trashed 64 KB when the runtime size happened to be 0.  The guarded
 * lowering wraps the block instructions in a runtime BC==0 guard.
 *
 * This test calls each of memcpy / memmove / memset with size==0
 * (non-constant — passed through a volatile to defeat compile-time
 * folding) and verifies that:
 *   - sentinel bytes BEFORE and AFTER the dst region survive,
 *   - the dst region itself is unchanged.
 */

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

extern void *memcpy(void *dest, const void *src, uint16_t n);
extern void *memmove(void *dest, const void *src, uint16_t n);
extern void *memset(void *p, int v, uint16_t n);

static const uint8_t pattern[16] = {
    0x18, 0x42, 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE,
    0x55, 0xAA, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
};

static volatile uint8_t before[8] = {
    0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
};
static volatile uint8_t dst[16] = {
    0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
    0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0,
};
static volatile uint8_t after[8] = {
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
};

static volatile uint16_t zero_size = 0;

static int sentinels_ok(void)
{
    static const uint8_t expected_before[8] = {
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    };
    static const uint8_t expected_after[8] = {
        0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
    };
    static const uint8_t expected_dst[16] = {
        0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
        0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0,
    };
    for (uint8_t i = 0; i < 8; i++) {
        if (before[i] != expected_before[i]) return 0;
        if (after[i]  != expected_after[i])  return 0;
    }
    for (uint8_t i = 0; i < 16; i++) {
        if (dst[i] != expected_dst[i]) return 0;
    }
    return 1;
}

int main(void)
{
    uint16_t status = 0;

    /* Bit 0: memcpy(dst, pattern, 0) must not modify anything. */
    memcpy((void *)dst, (const void *)pattern, zero_size);
    if (sentinels_ok()) status |= (1 << 0);

    /* Bit 1: memset(dst, 0xFF, 0) must not modify anything. */
    memset((void *)dst, 0xFF, zero_size);
    if (sentinels_ok()) status |= (1 << 1);

    /* Bit 2: memmove(dst, pattern, 0) must not modify anything. */
    memmove((void *)dst, (const void *)pattern, zero_size);
    if (sentinels_ok()) status |= (1 << 2);

    return status; /* expect 0x0007 */
}
