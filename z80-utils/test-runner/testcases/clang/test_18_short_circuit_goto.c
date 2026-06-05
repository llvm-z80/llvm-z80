/* Test 18: Short-circuit evaluation, goto forward/backward, goto cleanup */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

volatile uint8_t side_effect_counter;

uint8_t check_true(void) {
    side_effect_counter++;
    return 1;
}

uint8_t check_false(void) {
    side_effect_counter++;
    return 0;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: short-circuit && - second not evaluated when first is false */
    {
        side_effect_counter = 0;
        volatile uint8_t a = 0; /* false */
        /* When a is false (0), && short-circuits: check_true should NOT be called */
        if (a && check_true()) {
            /* should not enter */
        }
        /* counter should be 0 because check_true was never called */
        uint8_t c1 = side_effect_counter;

        /* Now verify && evaluates both when first is true */
        side_effect_counter = 0;
        volatile uint8_t b = 1; /* true */
        if (b && check_true()) {
            /* should enter */
        }
        /* counter should be 1 because check_true was called */
        uint8_t c2 = side_effect_counter;

        if (c1 == 0 && c2 == 1)
            status |= 1;
    }

    /* Bit 1: short-circuit || - second not evaluated when first is true */
    {
        side_effect_counter = 0;
        volatile uint8_t a = 1; /* true */
        /* When a is true (1), || short-circuits: check_false should NOT be called */
        if (a || check_false()) {
            /* should enter */
        }
        uint8_t c1 = side_effect_counter;

        /* Now verify || evaluates both when first is false */
        side_effect_counter = 0;
        volatile uint8_t b = 0; /* false */
        if (b || check_true()) {
            /* should enter */
        }
        uint8_t c2 = side_effect_counter;

        if (c1 == 0 && c2 == 1)
            status |= 2;
    }

    /* Bit 2: goto forward (skip code) and goto backward (loop sum 1..5=15) */
    {
        volatile uint16_t skipped = 0;
        goto skip_section;
        skipped = 999; /* this should be skipped */
skip_section:

        /* goto backward: sum 1..5 using goto loop */
        volatile uint16_t sum = 0;
        volatile uint16_t idx = 1;
loop_start:
        if (idx > 5) goto loop_end;
        sum += idx;
        idx++;
        goto loop_start;
loop_end:

        if (skipped == 0 && sum == 15)
            status |= 4;
    }

    /* Bit 3: goto for cleanup + goto to break nested loop */
    {
        /* Simulate allocate-then-error with cleanup */
        volatile uint8_t resource = 1; /* "allocated" */
        volatile uint8_t error = 1;    /* simulate error */
        volatile uint8_t cleaned = 0;

        if (error) goto cleanup;
        resource = 99; /* should be skipped */
        goto after_cleanup;
cleanup:
        cleaned = 1;
        resource = 0; /* "freed" */
after_cleanup:

        /* goto to break double-nested loop */
        volatile uint16_t found_i = 0, found_j = 0;
        for (uint8_t i = 0; i < 5; i++) {
            for (uint8_t j = 0; j < 5; j++) {
                if (i == 2 && j == 3) {
                    found_i = i;
                    found_j = j;
                    goto done_nested;
                }
            }
        }
done_nested:

        if (cleaned == 1 && resource == 0 && found_i == 2 && found_j == 3)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
