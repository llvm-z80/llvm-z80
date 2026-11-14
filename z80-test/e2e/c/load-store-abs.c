// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x000F %t

/* Test 8-bit load/store with constant address lowering to LD A,(nn) / LD (nn),A.
 * Verifies correctness across unsigned and signed globals, array offsets, and
 * consecutive global memory accesses. */

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;

volatile uint8_t g_u8 = 0x5A;
volatile int8_t g_s8 = -42;
volatile uint8_t g_arr[8] = {10, 20, 30, 40, 50, 60, 70, 80};
volatile uint8_t g_out = 0;

__attribute__((noinline))
void set_u8(uint8_t val) {
    g_u8 = val;
}

__attribute__((noinline))
uint8_t get_u8(void) {
    return g_u8;
}

__attribute__((noinline))
void copy_offset(void) {
    g_arr[1] = g_arr[5];
    g_arr[6] = g_arr[2] + 5;
}

__attribute__((noinline))
void multi_access(uint8_t a, uint8_t b) {
    g_u8 = a;
    g_out = b;
}

int main(void) {
    uint16_t status = 0;

    /* Bit 0: basic global uint8_t read/write */
    {
        uint8_t init = get_u8();
        set_u8(0xA5);
        uint8_t updated = get_u8();
        set_u8(init);
        if (init == 0x5A && updated == 0xA5)
            status |= 1;
    }

    /* Bit 1: global array with constant displacements */
    {
        copy_offset();
        if (g_arr[1] == 60 && g_arr[6] == 35)
            status |= 2;
    }

    /* Bit 2: consecutive global accesses and register assignment */
    {
        multi_access(0x12, 0x34);
        if (g_u8 == 0x12 && g_out == 0x34)
            status |= 4;
    }

    /* Bit 3: signed 8-bit global load with sign-extension */
    {
        int8_t s = g_s8;
        int16_t widened = s;
        g_s8 = -1;
        int8_t s2 = g_s8;
        if (s == -42 && widened == -42 && s2 == -1)
            status |= 8;
    }

    return status;
}
