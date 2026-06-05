/* Test 16: Switch-case - dense, sparse, fallthrough, switch in loop */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* Dense switch: map 0-5 to values */
uint16_t dense_map(uint16_t x) {
    switch (x) {
        case 0: return 10;
        case 1: return 20;
        case 2: return 30;
        case 3: return 40;
        case 4: return 50;
        case 5: return 60;
        default: return 0;
    }
}

/* Sparse switch: map 1/10/100/1000 */
uint16_t sparse_map(uint16_t x) {
    switch (x) {
        case 1:    return 11;
        case 10:   return 22;
        case 100:  return 33;
        case 1000: return 44;
        default:   return 99;
    }
}

/* Day classification with fallthrough: 1-5 weekday, 6-7 weekend */
uint8_t day_type(uint8_t day) {
    uint8_t result = 0;
    switch (day) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            result = 1; /* weekday */
            break;
        case 6:
        case 7:
            result = 2; /* weekend */
            break;
        default:
            result = 0; /* invalid */
            break;
    }
    return result;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: dense switch - verify all 6 cases + default */
    {
        volatile uint16_t v0 = 0, v1 = 1, v2 = 2, v3 = 3, v4 = 4, v5 = 5;
        volatile uint16_t vd = 9;
        uint8_t ok = 1;
        if (dense_map(v0) != 10) ok = 0;
        if (dense_map(v1) != 20) ok = 0;
        if (dense_map(v2) != 30) ok = 0;
        if (dense_map(v3) != 40) ok = 0;
        if (dense_map(v4) != 50) ok = 0;
        if (dense_map(v5) != 60) ok = 0;
        if (dense_map(vd) != 0)  ok = 0;
        if (ok) status |= 1;
    }

    /* Bit 1: sparse switch - verify each + default */
    {
        volatile uint16_t s1 = 1, s10 = 10, s100 = 100, s1000 = 1000, sd = 500;
        uint8_t ok = 1;
        if (sparse_map(s1) != 11)   ok = 0;
        if (sparse_map(s10) != 22)  ok = 0;
        if (sparse_map(s100) != 33) ok = 0;
        if (sparse_map(s1000) != 44) ok = 0;
        if (sparse_map(sd) != 99)   ok = 0;
        if (ok) status |= 2;
    }

    /* Bit 2: switch with fallthrough - day classification */
    {
        volatile uint8_t d1 = 1, d3 = 3, d5 = 5, d6 = 6, d7 = 7, d0 = 0;
        uint8_t ok = 1;
        if (day_type(d1) != 1) ok = 0;  /* weekday */
        if (day_type(d3) != 1) ok = 0;  /* weekday */
        if (day_type(d5) != 1) ok = 0;  /* weekday */
        if (day_type(d6) != 2) ok = 0;  /* weekend */
        if (day_type(d7) != 2) ok = 0;  /* weekend */
        if (day_type(d0) != 0) ok = 0;  /* invalid */
        if (ok) status |= 4;
    }

    /* Bit 3: switch in loop - count categories in array */
    {
        volatile uint8_t items[8];
        items[0] = 1; items[1] = 2; items[2] = 1; items[3] = 3;
        items[4] = 2; items[5] = 1; items[6] = 3; items[7] = 2;
        /* category 1 appears 3 times, category 2 appears 3 times, category 3 appears 2 times */

        uint8_t cat1 = 0, cat2 = 0, cat3 = 0;
        for (uint8_t i = 0; i < 8; i++) {
            switch (items[i]) {
                case 1: cat1++; break;
                case 2: cat2++; break;
                case 3: cat3++; break;
                default: break;
            }
        }
        if (cat1 == 3 && cat2 == 3 && cat3 == 2)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
