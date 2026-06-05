/* Test 49: __attribute__((interrupt)) - ISR register preservation via inline asm */
/* Uses inline asm "call" to bypass compiler's caller-save convention,
   directly testing that the interrupt handler pushes/pops all registers. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t isr_byte_result;
volatile uint8_t reg_result_a;
volatile uint8_t reg_result_b;
volatile uint8_t reg_result_d;

/* ISR that deliberately clobbers A, B, D, E registers.
   With __attribute__((interrupt)), prologue/epilogue saves and restores them. */
__attribute__((interrupt)) void isr_clobber_regs(void) {
    asm volatile(
        "ld a, #0xFF\n\t"
        "ld b, #0xFF\n\t"
        "ld d, #0xFF\n\t"
        "ld e, #0xFF\n\t"
        ::: "a", "b", "d", "e"
    );
}

/* Simple ISR for basic functionality test */
__attribute__((interrupt)) void isr_byte_store(void) {
    isr_byte_result = 0x42;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: basic ISR stores byte correctly */
    {
        isr_byte_result = 0;
        isr_byte_store();
        if (isr_byte_result == 0x42)
            status |= 1;
    }

    /* Bits 1-3: register preservation test via inline asm call.
       We set known values in A/B/D, call the ISR directly via asm "call"
       (bypassing compiler's caller-save), then verify registers are preserved.
       This only works because the ISR has __attribute__((interrupt)) which
       generates push/pop for all register pairs. */
    {
        asm volatile(
            "ld a, #0x42\n\t"
            "ld b, #0x55\n\t"
            "ld d, #0xAA\n\t"
            "call _isr_clobber_regs\n\t"
            "ld (_reg_result_a), a\n\t"
            "ld a, b\n\t"
            "ld (_reg_result_b), a\n\t"
            "ld a, d\n\t"
            "ld (_reg_result_d), a\n\t"
            ::: "a", "b", "c", "d", "e", "h", "l", "memory"
        );

        if (reg_result_a == 0x42) status |= 2;
        if (reg_result_b == 0x55) status |= 4;
        if (reg_result_d == 0xAA) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
