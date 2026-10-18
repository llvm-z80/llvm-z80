// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x007F %t

/* memcpy and memmove lowered to LDIR and LDDR, with constant, run-time and
 * zero lengths and with overlaps in both directions. LDIR and LDDR copy 64 KB
 * when BC is 0, so a run-time length needs a zero guard. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

static uint8_t src_buf[16];
static uint8_t dst_buf[16];

/* Canaries around dst_buf. A copy with BC == 0 overwrites the canaries. */
static uint8_t guard_lo[4];
static uint8_t guard_hi[4];

/* One buffer for the overlapping cases, so the copy direction decides the
 * result. */
static uint8_t ovl[24];

static void fill_src(void) {
    uint16_t i;
    for (i = 0; i < 16; i++)
        src_buf[i] = (uint8_t)(i + 1);
}

static void clear_dst(void) {
    uint16_t i;
    for (i = 0; i < 16; i++)
        dst_buf[i] = 0;
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

static void fill_ovl(void) {
    uint16_t i;
    for (i = 0; i < 24; i++)
        ovl[i] = (uint8_t)(i + 1);
}

int main(void) {
    volatile uint16_t status = 0;
    uint16_t i;

    /* Bit 0: constant length, lowered to a bare LDIR. */
    {
        int ok = 1;
        fill_src();
        clear_dst();
        __builtin_memcpy(dst_buf, src_buf, 12);
        for (i = 0; i < 12; i++)
            if (dst_buf[i] != (uint8_t)(i + 1))
                ok = 0;
        for (i = 12; i < 16; i++)
            if (dst_buf[i] != 0)
                ok = 0; /* copied past the end */
        if (ok && guards_intact())
            status |= (1 << 0);
    }

    /* Bit 1: run-time length of zero, skipped by the guard. */
    {
        volatile uint16_t n = 0;
        int ok = 1;
        fill_src();
        clear_dst();
        __builtin_memcpy(dst_buf, src_buf, n);
        for (i = 0; i < 16; i++)
            if (dst_buf[i] != 0)
                ok = 0;
        if (ok && guards_intact())
            status |= (1 << 1);
    }

    /* Bit 2: nonzero run-time length through the same guard. */
    {
        volatile uint16_t n = 8;
        int ok = 1;
        fill_src();
        clear_dst();
        __builtin_memcpy(dst_buf, src_buf, n);
        for (i = 0; i < 8; i++)
            if (dst_buf[i] != (uint8_t)(i + 1))
                ok = 0;
        for (i = 8; i < 16; i++)
            if (dst_buf[i] != 0)
                ok = 0;
        if (ok && guards_intact())
            status |= (1 << 2);
    }

    /* Bit 3: overlapping memmove with the destination below the source,
     * copied ascending. */
    {
        int ok = 1;
        fill_ovl();
        __builtin_memmove(&ovl[0], &ovl[4], 16);
        for (i = 0; i < 16; i++)
            if (ovl[i] != (uint8_t)(i + 5))
                ok = 0;
        for (i = 16; i < 24; i++)
            if (ovl[i] != (uint8_t)(i + 1))
                ok = 0; /* tail must be untouched */
        if (ok)
            status |= (1 << 3);
    }

    /* Bit 4: overlapping memmove with the destination above the source,
     * copied descending. */
    {
        int ok = 1;
        fill_ovl();
        __builtin_memmove(&ovl[4], &ovl[0], 16);
        for (i = 4; i < 20; i++)
            if (ovl[i] != (uint8_t)(i - 3))
                ok = 0;
        for (i = 0; i < 4; i++)
            if (ovl[i] != (uint8_t)(i + 1))
                ok = 0; /* head must be untouched */
        if (ok)
            status |= (1 << 4);
    }

    /* Bit 5: the same two overlaps through pointers the compiler cannot
     * relate, so the library routine picks the direction at run time. */
    {
        int ok = 1;
        uint8_t *volatile lo = &ovl[0];
        uint8_t *volatile hi = &ovl[4];

        fill_ovl();
        __builtin_memmove(lo, hi, 16);
        for (i = 0; i < 16; i++)
            if (ovl[i] != (uint8_t)(i + 5))
                ok = 0;

        fill_ovl();
        __builtin_memmove(hi, lo, 16);
        for (i = 4; i < 20; i++)
            if (ovl[i] != (uint8_t)(i - 3))
                ok = 0;

        if (ok)
            status |= (1 << 5);
    }

    /* Bit 6: a known direction with a run-time length, zero and nonzero. */
    {
        volatile uint16_t n = 0;
        int ok = 1;
        fill_ovl();
        __builtin_memmove(&ovl[0], &ovl[4], n);
        for (i = 0; i < 24; i++)
            if (ovl[i] != (uint8_t)(i + 1))
                ok = 0;

        n = 16;
        fill_ovl();
        __builtin_memmove(&ovl[0], &ovl[4], n);
        for (i = 0; i < 16; i++)
            if (ovl[i] != (uint8_t)(i + 5))
                ok = 0;

        if (ok && guards_intact())
            status |= (1 << 6);
    }

    return status;
}
