/* Test 57: SM83 inline assembly */
/* SKIP-IF: z80 */
/* SKIP-IF: -fno-integrated-as */
/* expect: 0x00FF */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t g_byte;

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

/* Bit 1: ALU ops (add, and, or, xor) */
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

/* Bit 2: swap nibbles (SM83-only instruction) */
uint8_t test_swap(void) {
    uint8_t result;
    asm volatile(
        "ld a, #0xAB\n\t"
        "swap a\n\t"          /* a = 0xBA */
        : "=a"(result)
    );
    return result;
}

/* Bit 3: (hl) indirect load */
uint8_t test_hl_indirect_load(void) {
    volatile uint8_t val = 0xCD;
    uint8_t result;
    register uint8_t *ptr asm("hl") = (uint8_t *)&val;
    asm volatile(
        "ld a, (hl)\n\t"
        : "=a"(result)
        : "r"(ptr)
        : "memory"
    );
    return result;
}

/* Bit 4: (hl) indirect store */
uint8_t test_hl_indirect_store(void) {
    volatile uint8_t val = 0;
    register uint8_t *ptr asm("hl") = (uint8_t *)&val;
    asm volatile(
        "ld a, #0x77\n\t"
        "ld (hl), a\n\t"
        :
        : "r"(ptr)
        : "a", "memory"
    );
    return val;
}

/* Bit 5: 16-bit register loads and byte extraction */
uint8_t test_16bit_regs(void) {
    uint8_t result;
    asm volatile(
        "ld de, #0x1234\n\t"
        "ld a, d\n\t"         /* a = 0x12 */
        "add a, e\n\t"        /* a = 0x12 + 0x34 = 0x46 */
        : "=a"(result)
        :
        : "de"
    );
    return result;
}

/* Bit 6: push/pop register pair */
uint8_t test_push_pop(void) {
    uint8_t result;
    asm volatile(
        "ld a, #0x55\n\t"
        "ld b, a\n\t"
        "push bc\n\t"
        "ld b, #0x00\n\t"
        "pop bc\n\t"
        "ld a, b\n\t"        /* a = 0x55 */
        : "=a"(result)
        :
        : "bc"
    );
    return result;
}

/* Bit 7: call/ret with local labels */
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

    if (test_swap() == 0xBA)
        status |= (1 << 2);

    if (test_hl_indirect_load() == 0xCD)
        status |= (1 << 3);

    if (test_hl_indirect_store() == 0x77)
        status |= (1 << 4);

    if (test_16bit_regs() == 0x46)
        status |= (1 << 5);

    if (test_push_pop() == 0x55)
        status |= (1 << 6);

    if (test_asm_call() == 0x99)
        status |= (1 << 7);

    return status; /* expect 0x00FF */
}
