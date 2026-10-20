// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x003F %t/prog

//--- clang.c
/* Clang main and SDCC functions share i16 and i32 globals defined in the
 * SDCC half. Clang main also checks initialized and zeroed Clang globals. */
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

volatile u16 startup_counter = 42;
volatile u16 startup_buf[64];

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

    /* Bit 3: both sides modify the globals */
    {
        sdcc_reset();
        shared_counter = 1000;
        sdcc_inc_counter(500);
        shared_accum = 0x12340000UL;
        sdcc_add_accum(0x5678UL);
        if (shared_counter == 1500 && shared_accum == 0x12345678UL)
            status |= (1 << 3);
    }

    /* Bit 4: startup_counter holds the initial value 42 */
    if (startup_counter == 42) {
        status |= (1 << 4);
    }

    /* Bit 5: startup_buf is all zero */
    {
        int all_zero = 1;
        for (int i = 0; i < 64; i++) {
            if (startup_buf[i] != 0) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero)
            status |= (1 << 5);
    }

    return status;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u16 shared_counter;
u32 shared_accum;

void sdcc_reset(void) {
    shared_counter = 0;
    shared_accum = 0;
}

void sdcc_inc_counter(u16 n) {
    shared_counter += n;
}

void sdcc_add_accum(u32 val) {
    shared_accum += val;
}

u16 sdcc_get_counter(void) {
    return shared_counter;
}

u32 sdcc_get_accum(void) {
    return shared_accum;
}
