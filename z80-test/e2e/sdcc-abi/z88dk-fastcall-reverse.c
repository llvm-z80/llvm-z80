// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -Wl,-l,%{sdcc-lib} -o %t/prog
// RUN: %run --expect-result=0x0007 %t/prog
// UNSUPPORTED: sm83

//--- clang.c
/* SDCC main calls Clang __z88dk_fastcall functions with a single i8, i16, or
 * i32 argument in L, HL or DEHL. Neither compiler accepts __z88dk_fastcall
 * for SM83. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

__attribute__((z88dk_fastcall)) u8 clang_fc_i8(u8 a) {
    return (u8)(a + 1u);
}

__attribute__((z88dk_fastcall)) u16 clang_fc_i16(u16 a) {
    return (u16)(a + 1u);
}

__attribute__((z88dk_fastcall)) u32 clang_fc_i32(u32 a) {
    return a + 1UL;
}

/* i8 in, i16 out: the argument is only L and the result is all of HL. */
__attribute__((z88dk_fastcall)) u16 clang_fc_i8_widen(u8 a) {
    return (u16)(a * 100u);
}

/* i32 in, i8 out: only the high byte of DE survives into L. */
__attribute__((z88dk_fastcall)) u8 clang_fc_i32_narrow(u32 a) {
    return (u8)(a >> 24);
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

extern u8 clang_fc_i8(u8 a) __z88dk_fastcall;
extern u16 clang_fc_i16(u16 a) __z88dk_fastcall;
extern u32 clang_fc_i32(u32 a) __z88dk_fastcall;
extern u16 clang_fc_i8_widen(u8 a) __z88dk_fastcall;
extern u8 clang_fc_i32_narrow(u32 a) __z88dk_fastcall;

int main(void) {
    u16 status = 0;

    /* Bit 0: i8 in L and i16 in HL. */
    {
        u8 r1 = clang_fc_i8(41);
        u16 r2 = clang_fc_i16(9999);
        if (r1 == 42 && r2 == 10000)
            status |= (1 << 0);
    }

    /* Bit 1: i32 in DEHL, and an i32 return in DEHL, not the default HLDE. */
    {
        u32 r = clang_fc_i32(0x11223344UL);
        if (r == 0x11223345UL)
            status |= (1 << 1);
    }

    /* Bit 2: width changes between the argument and the result. */
    {
        u16 r1 = clang_fc_i8_widen(9);
        u8 r2 = clang_fc_i32_narrow(0xAB000000UL);
        if (r1 == 900 && r2 == 0xAB)
            status |= (1 << 2);
    }

    return status;
}
