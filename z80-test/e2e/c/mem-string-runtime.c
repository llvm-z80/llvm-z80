// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x00FF %t

/* The memory and string runtime routines at edge cases: lengths of 0, 1 and
 * either side of 256, and comparisons that need the sign of an unsigned byte
 * compare. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

int bcmp(const void *, const void *, uint16_t);

static uint8_t src[300];
static uint8_t dst[300];
static uint8_t guard_lo[4];
static uint8_t guard_hi[4];

static void fill_src(void) {
    uint16_t i;
    for (i = 0; i < 300; i++)
        src[i] = (uint8_t)(i * 7 + 1);
}

static void clear_dst(void) {
    uint16_t i;
    for (i = 0; i < 300; i++)
        dst[i] = 0;
    for (i = 0; i < 4; i++) {
        guard_lo[i] = 0x5A;
        guard_hi[i] = 0xA5;
    }
}

static int guards_intact(void) {
    uint16_t i;
    for (i = 0; i < 4; i++)
        if (guard_lo[i] != 0x5A || guard_hi[i] != 0xA5)
            return 0;
    return 1;
}

/* Copies n bytes and checks that exactly n bytes moved. */
static int check_copy(uint16_t n) {
    volatile uint16_t len = n;   /* opaque, so the length reaches the runtime */
    uint16_t i;
    fill_src();
    clear_dst();
    __builtin_memcpy(dst, src, len);
    for (i = 0; i < n; i++)
        if (dst[i] != src[i])
            return 0;
    for (i = n; i < 300; i++)
        if (dst[i] != 0)
            return 0;
    return guards_intact();
}

static int check_fill(uint16_t n) {
    volatile uint16_t len = n;
    uint16_t i;
    clear_dst();
    __builtin_memset(dst, 0xC3, len);
    for (i = 0; i < n; i++)
        if (dst[i] != 0xC3)
            return 0;
    for (i = n; i < 300; i++)
        if (dst[i] != 0)
            return 0;
    return guards_intact();
}

int main(void) {
    volatile uint16_t status = 0;
    uint16_t i;

    /* Bit 0: copy lengths of 0, 1, 2 and around 256. */
    if (check_copy(0) && check_copy(1) && check_copy(2) && check_copy(255) &&
        check_copy(256) && check_copy(257) && check_copy(300))
        status |= (1 << 0);

    /* Bit 1: fill lengths around the same boundaries. */
    if (check_fill(0) && check_fill(1) && check_fill(255) && check_fill(256) &&
        check_fill(257) && check_fill(300))
        status |= (1 << 1);

    /* Bit 2: overlapping moves in both directions, at a length that crosses
     * the 256-byte boundary. */
    {
        int ok = 1;
        volatile uint16_t len = 260;
        fill_src();
        for (i = 0; i < 300; i++)
            dst[i] = src[i];
        __builtin_memmove(&dst[0], &dst[8], len);   /* down: ascending works */
        for (i = 0; i < 260; i++)
            if (dst[i] != src[i + 8])
                ok = 0;

        for (i = 0; i < 300; i++)
            dst[i] = src[i];
        __builtin_memmove(&dst[8], &dst[0], len);   /* up: must go descending */
        for (i = 8; i < 268; i++)
            if (dst[i] != src[i - 8])
                ok = 0;
        for (i = 0; i < 8; i++)
            if (dst[i] != src[i])
                ok = 0;
        if (ok)
            status |= (1 << 2);
    }

    /* Bit 3: memcmp carries the sign of an unsigned byte comparison, also for
     * 0x00 against 0xFF. */
    {
        volatile uint16_t n = 4;
        int ok = 1;
        static uint8_t a[4], b[4];
        a[0] = 0x00; a[1] = 0; a[2] = 0; a[3] = 0;
        b[0] = 0xFF; b[1] = 0; b[2] = 0; b[3] = 0;
        if (__builtin_memcmp(a, b, n) >= 0) ok = 0;
        a[0] = 0xFF; b[0] = 0x00;
        if (__builtin_memcmp(a, b, n) <= 0) ok = 0;
        a[0] = 0x7F; b[0] = 0x80;
        if (__builtin_memcmp(a, b, n) >= 0) ok = 0;
        a[0] = 0x01; b[0] = 0x02;
        if (__builtin_memcmp(a, b, n) >= 0) ok = 0;
        a[0] = 0x42; b[0] = 0x42;
        if (__builtin_memcmp(a, b, n) != 0) ok = 0;
        if (ok)
            status |= (1 << 3);
    }

    /* Bit 4: memcmp over a length past 256, differing only in the last byte. */
    {
        volatile uint16_t n = 300;
        int ok = 1;
        fill_src();
        for (i = 0; i < 300; i++)
            dst[i] = src[i];
        if (__builtin_memcmp(dst, src, n) != 0) ok = 0;
        dst[299] = (uint8_t)(src[299] + 1);
        if (__builtin_memcmp(dst, src, n) <= 0) ok = 0;
        dst[299] = (uint8_t)(src[299] - 1);
        if (__builtin_memcmp(dst, src, n) >= 0) ok = 0;
        if (ok)
            status |= (1 << 4);
    }

    /* Bit 5: strcmp and strncmp carry the same unsigned sign. */
    {
        static char s1[4], s2[4];
        volatile uint16_t n = 4;
        int ok = 1;
        s1[0] = 0x01; s1[1] = 0; s2[0] = (char)0xFF; s2[1] = 0;
        if (__builtin_strcmp(s1, s2) >= 0) ok = 0;
        if (__builtin_strncmp(s1, s2, n) >= 0) ok = 0;
        s1[0] = (char)0xFF; s2[0] = 0x01;
        if (__builtin_strcmp(s1, s2) <= 0) ok = 0;
        if (__builtin_strncmp(s1, s2, n) <= 0) ok = 0;
        s1[0] = 0x41; s2[0] = 0x41;
        if (__builtin_strcmp(s1, s2) != 0) ok = 0;
        if (__builtin_strncmp(s1, s2, n) != 0) ok = 0;
        if (ok)
            status |= (1 << 5);
    }

    /* Bit 6: strncmp stops at n, and a zero n compares equal whatever the
     * strings hold. */
    {
        static char s1[8], s2[8];
        volatile uint16_t n0 = 0, n3 = 3;
        int ok = 1;
        __builtin_memcpy(s1, "abcX", 5);
        __builtin_memcpy(s2, "abcY", 5);
        if (__builtin_strncmp(s1, s2, n0) != 0) ok = 0;
        if (__builtin_strncmp(s1, s2, n3) != 0) ok = 0;
        if (__builtin_strncmp(s1, s2, (uint16_t)4) >= 0) ok = 0;
        if (ok)
            status |= (1 << 6);
    }

    /* Bit 7: bcmp agrees with memcmp on equal and not equal, and takes a zero
     * length as equal. */
    {
        static uint8_t a[8], b[8];
        volatile uint16_t n0 = 0, n7 = 7, n8 = 8;
        int ok = 1;
        for (i = 0; i < 8; i++) { a[i] = (uint8_t)(i + 1); b[i] = a[i]; }
        if (bcmp(a, b, n8) != 0) ok = 0;
        if (bcmp(a, b, n0) != 0) ok = 0;
        b[7] = 9;                       /* differs at the last byte */
        if (bcmp(a, b, n8) == 0) ok = 0;
        if (bcmp(a, b, n7) != 0) ok = 0;
        if ((bcmp(a, b, n8) == 0) != (__builtin_memcmp(a, b, 8) == 0)) ok = 0;
        b[7] = 8; b[0] = 9;             /* differs at the first byte */
        if (bcmp(a, b, n8) == 0) ok = 0;
        if (bcmp(a, b, (uint16_t)1) == 0) ok = 0;
        if (ok)
            status |= (1 << 7);
    }

    return status;
}
