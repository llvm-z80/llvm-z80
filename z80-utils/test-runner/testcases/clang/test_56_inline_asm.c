/* Test 56: Inline assembly - both LLVM and sdasz80 syntax */
/* SKIP-IF: sm83 */
/* SKIP-IF: -fno-integrated-as */
/* expect: 0x00FF */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t g_byte;
volatile uint16_t g_word;

/* Bit 0: basic register move via inline asm */
uint8_t test_reg_move(void) {
    uint8_t result;
    asm volatile(
        "ld a, #0x42\n\t"
        "ld b, a\n\t"
        "ld a, b\n\t"
        : "=a"(result)
        :
        : "b"
    );
    return result;
}

/* Bit 1: arithmetic in asm (add, sub, and, or, xor) */
uint8_t test_asm_arith(void) {
    uint8_t result;
    asm volatile(
        "ld a, #0xFF\n\t"
        "and a, #0x0F\n\t"   /* a = 0x0F */
        "or a, #0x30\n\t"    /* a = 0x3F */
        "xor a, #0x36\n\t"   /* a = 0x09 */
        "add a, #0x01\n\t"   /* a = 0x0A */
        : "=a"(result)
    );
    return result;
}

/* Bit 2: memory store via direct addressing ld (nn), a */
uint8_t test_mem_store(void) {
    g_byte = 0;
    asm volatile(
        "ld a, #0xAB\n\t"
        "ld (_g_byte), a\n\t"
        ::: "a", "memory"
    );
    return g_byte;
}

/* Bit 3: memory load via direct addressing ld a, (nn) */
uint8_t test_mem_load(void) {
    g_byte = 0xCD;
    uint8_t result;
    asm volatile(
        "ld a, (_g_byte)\n\t"
        : "=a"(result)
        :: "memory"
    );
    return result;
}

/* Bit 4: 16-bit direct store/load ld (nn), hl / ld hl, (nn) */
uint16_t test_mem16(void) {
    g_word = 0;
    asm volatile(
        "ld hl, #0xBEEF\n\t"
        "ld (_g_word), hl\n\t"
        ::: "hl", "memory"
    );
    return g_word;
}

/* Bit 5: register indirect (hl) load and store */
uint8_t test_hl_indirect(void) {
    volatile uint8_t val = 0x77;
    register uint8_t *ptr asm("hl") = (uint8_t *)&val;
    asm volatile(
        "ld a, #0x00\n\t"
        "ld a, (hl)\n\t"
        "ld (_g_byte), a\n\t"
        :
        : "r"(ptr)
        : "a", "memory"
    );
    return g_byte;
}

/* Bit 6: indexed addressing (ix+d) */
uint8_t test_ix_indexed(void) {
    volatile uint8_t arr[4] = {0x11, 0x22, 0x33, 0x44};
    return arr[2]; /* should return 0x33 */
}

/* Bit 7: asm with labels (call/ret, jp) using LLVM local labels */
uint8_t test_asm_call(void) {
    uint8_t result;
    asm volatile(
        "ld a, #0x00\n\t"
        "call 1f\n\t"
        "jr 2f\n\t"
        "1:\n\t"
        "ld a, #0x99\n\t"
        "ret\n\t"
        "2:\n\t"
        : "=a"(result)
        :
        : "memory"
    );
    return result;
}

int main(void) {
    uint16_t status = 0;

    if (test_reg_move() == 0x42)
        status |= (1 << 0);

    if (test_asm_arith() == 0x0A)
        status |= (1 << 1);

    if (test_mem_store() == 0xAB)
        status |= (1 << 2);

    if (test_mem_load() == 0xCD)
        status |= (1 << 3);

    if (test_mem16() == 0xBEEF)
        status |= (1 << 4);

    if (test_hl_indirect() == 0x77)
        status |= (1 << 5);

    if (test_ix_indexed() == 0x33)
        status |= (1 << 6);

    if (test_asm_call() == 0x99)
        status |= (1 << 7);

    return status; /* expect 0x00FF */
}
