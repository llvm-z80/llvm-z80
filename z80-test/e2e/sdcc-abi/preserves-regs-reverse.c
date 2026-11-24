// RUN: rm -rf %t && split-file %s %t
// RUN: %{sdcc} -c %t/sdcc.c -o %t/sdcc.rel
// RUN: %cc %t/clang.c %t/sdcc.rel -o %t/prog
// RUN: %run --expect-result=0x0003 %t/prog

//--- clang.c
/* SDCC main calls Clang functions that preserve registers. SDCC keeps values
 * in them across the call, so the bodies have to restore them. */
typedef unsigned char u8;
typedef unsigned short u16;

volatile u16 sink;

__attribute__((noinline)) static u16 churn(u16 a, u16 b) {
    u16 r = 0;
    for (u8 i = 0; i < 8; i++)
        r = (u16)(r << 1) ^ (u16)(a + i) ^ (u16)(b >> i);
    return r;
}

void touch(u16 x) __attribute__((preserves_regs(b, c, d, e, h, l))) {
    sink = churn(x, x + 3) ^ churn(x + 5, x);
}

u8 touch8(u8 x) __attribute__((preserves_regs(b, c, d, e, h, l))) {
    sink = churn(x, 7);
    return x + 1;
}

//--- sdcc.c
typedef unsigned char u8;
typedef unsigned short u16;

extern void touch(u16 x) __preserves_regs(b, c, d, e, h, l);
extern u8 touch8(u8 x) __preserves_regs(b, c, d, e, h, l);

int main(void)
{
    u16 status = 0;
    u16 acc = 0;
    u8 i;

    /* Bit 0: SDCC's loop state survives a void call */
    for (i = 0; i < 10; i++) {
        touch(i);
        acc += i;
    }
    if (acc == 45)
        status |= (1 << 0);

    /* Bit 1: and a call that returns in A */
    acc = 0;
    for (i = 0; i < 10; i++)
        acc += touch8(i);
    if (acc == 55)
        status |= (1 << 1);

    return status;
}
