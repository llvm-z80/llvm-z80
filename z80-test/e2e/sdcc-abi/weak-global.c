// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x0007 %t/prog

//--- clang.c
/* Clang main checks weak Clang definitions in the sdasz80 format, and an SDCC
 * function reads the weak data. */
typedef unsigned short u16;

volatile u16 weak_data __attribute__((weak)) = 0x1234;
__attribute__((weak)) u16 weak_func(void) { return 7; }

extern u16 sdcc_read_weak_data(void);

int main(void) {
    volatile u16 status = 0;

    /* Bit 0: the initial value is in place at startup. */
    if (weak_data == 0x1234)
        status |= (1 << 0);

    /* Bit 1: SDCC code reads the same variable. */
    if (sdcc_read_weak_data() == 0x1234)
        status |= (1 << 1);

    /* Bit 2: a weak function is called like any other. */
    if (weak_func() == 7)
        status |= (1 << 2);

    return status;
}

//--- sdcc.c
typedef unsigned short u16;

extern volatile u16 weak_data;

u16 sdcc_read_weak_data(void) { return weak_data; }
