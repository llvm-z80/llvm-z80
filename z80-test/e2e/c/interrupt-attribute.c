// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x000F %t

/* Register preservation in __attribute__((interrupt)) handlers. The handler
   is called from inline asm, so the caller saves no registers. */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t isr_byte_result;
volatile uint8_t reg_result_a;
volatile uint8_t reg_result_b;
volatile uint8_t reg_result_d;

/* ISR that clobbers A, B, D and E. */
__attribute__((interrupt)) void isr_clobber_regs(void) {
    asm volatile(
        "ld a, #0xFF\n\t"
        "ld b, #0xFF\n\t"
        "ld d, #0xFF\n\t"
        "ld e, #0xFF\n\t"
        ::: "a", "b", "d", "e"
    );
}

/* Simple ISR for the basic check. */
__attribute__((interrupt)) void isr_byte_store(void) {
    isr_byte_result = 0x42;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: basic ISR stores 0x42 */
    {
        isr_byte_result = 0;
        isr_byte_store();
        if (isr_byte_result == 0x42)
            status |= 1;
    }

    /* Bits 1-3: A, B and D survive an asm call to the ISR */
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

    return status;
}
