/* Test 08: Shared global variables across SDCC/Clang boundary */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u16 shared_counter;
extern u32 shared_accum;

extern void sdcc_reset(void);
extern void sdcc_inc_counter(u16 n);
extern void sdcc_add_accum(u32 val);
extern u16 sdcc_get_counter(void);
extern u32 sdcc_get_accum(void);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: Clang writes global, SDCC reads */
    {
        shared_counter = 42;
        volatile u16 r = sdcc_get_counter();
        if (r == 42)
            status |= (1 << 0);
    }

    /* Bit 1: SDCC writes global, Clang reads */
    {
        sdcc_reset();
        sdcc_inc_counter(100);
        sdcc_inc_counter(200);
        if (shared_counter == 300)
            status |= (1 << 1);
    }

    /* Bit 2: i32 global across boundary */
    {
        sdcc_reset();
        sdcc_add_accum(0x10000UL);
        sdcc_add_accum(0x20000UL);
        if (shared_accum == 0x30000UL)
            status |= (1 << 2);
    }

    /* Bit 3: mixed access - both sides modify */
    {
        sdcc_reset();
        shared_counter = 1000;
        sdcc_inc_counter(500);
        shared_accum = 0x12340000UL;
        sdcc_add_accum(0x5678UL);
        if (shared_counter == 1500 && shared_accum == 0x12345678UL)
            status |= (1 << 3);
    }

    return status; /* expect 0x000F */
}
