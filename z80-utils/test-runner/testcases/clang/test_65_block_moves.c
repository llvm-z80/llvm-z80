/* Test 65: Block moves - the memcpy/memmove lowering to LDIR and LDDR.
 *
 * LDIR and LDDR decrement BC before testing it for zero, so entering either
 * with BC == 0 copies 65536 bytes and flattens the whole address space.  A
 * length the compiler can prove non-zero goes straight to the instruction, a
 * runtime length goes through a guard that branches around it, and a constant
 * zero disappears entirely.  memmove additionally picks the direction: LDIR
 * ascending when the destination is below the source, LDDR descending when it
 * is above, and the library routine when neither can be proven.
 *
 * Every bit below is arranged so that a wrong direction corrupts the overlap
 * instead of merely reordering it, and the guard cases carry canaries on both
 * sides of the destination because the failure mode there is a 64 KB write
 * rather than a wrong byte. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

static uint8_t src_buf[16];
static uint8_t dst_buf[16];

/* Neighbours of dst_buf are checked after every guarded case: a BC == 0 run
 * would scribble over them long before it wrapped back around. */
static uint8_t guard_lo[4];
static uint8_t guard_hi[4];

/* One buffer for the overlapping cases, so source and destination share a
 * base and the direction is what decides the result. */
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

    /* Bit 0: constant length, which lowers to a bare LDIR. */
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

    /* Bit 1: runtime length of zero.  Reaching the check at all means the
     * guard branched around the LDIR; without it this writes 65536 bytes. */
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

    /* Bit 2: runtime length that is not zero, through the same guard. */
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

    /* Bit 3: overlapping memmove with the destination BELOW the source, which
     * is safe to copy ascending.  Copying descending here would read bytes it
     * had already overwritten. */
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

    /* Bit 4: overlapping memmove with the destination ABOVE the source, which
     * has to be copied descending. */
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
     * relate, so the direction is decided at run time by the library routine
     * rather than at compile time. */
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

    /* Bit 6: a known direction with a runtime length, including zero.  The
     * direction is folded at compile time but the guard still has to be
     * there. */
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

    return status; /* expect 0x007F */
}
