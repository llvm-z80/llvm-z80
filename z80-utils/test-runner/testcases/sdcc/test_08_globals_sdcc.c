/* SDCC functions for shared global variable tests */
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
