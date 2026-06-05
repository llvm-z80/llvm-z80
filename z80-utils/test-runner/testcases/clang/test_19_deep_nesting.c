/* Test 19: Deep nesting - nested if-else, nested loops, nested switch, mixed */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* 5-level nested if-else: classify into ranges with sub-ranges */
/* 1-10: sub 1-5->1, 6-10->2; 11-20: sub 11-15->3, 16-20->4; else->0 */
uint8_t deep_classify(int16_t x) {
    if (x >= 1 && x <= 20) {
        if (x <= 10) {
            if (x <= 5) {
                if (x <= 3) {
                    if (x == 1) return 10;
                    else return 11;
                } else {
                    return 12;
                }
            } else {
                return 2;
            }
        } else {
            if (x <= 15) {
                return 3;
            } else {
                return 4;
            }
        }
    } else {
        return 0;
    }
}

/* Nested switch: outer=category(1,2), inner=subcategory(1,2,3) */
uint16_t nested_switch(uint8_t cat, uint8_t subcat) {
    uint16_t result = 0;
    switch (cat) {
        case 1:
            switch (subcat) {
                case 1: result = 11; break;
                case 2: result = 12; break;
                case 3: result = 13; break;
                default: result = 10; break;
            }
            break;
        case 2:
            switch (subcat) {
                case 1: result = 21; break;
                case 2: result = 22; break;
                case 3: result = 23; break;
                default: result = 20; break;
            }
            break;
        default:
            result = 0;
            break;
    }
    return result;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: 5-level nested if-else */
    {
        volatile int16_t v1 = 1, v2 = 3, v5 = 5, v8 = 8, v13 = 13, v18 = 18, v25 = 25;
        uint8_t ok = 1;
        if (deep_classify(v1) != 10)  ok = 0;  /* x==1 */
        if (deep_classify(v2) != 11)  ok = 0;  /* x<=3, x!=1 */
        if (deep_classify(v5) != 12)  ok = 0;  /* x<=5, x>3 */
        if (deep_classify(v8) != 2)   ok = 0;  /* x<=10, x>5 */
        if (deep_classify(v13) != 3)  ok = 0;  /* 11-15 */
        if (deep_classify(v18) != 4)  ok = 0;  /* 16-20 */
        if (deep_classify(v25) != 0)  ok = 0;  /* out of range */
        if (ok) status |= 1;
    }

    /* Bit 1: 4-level nested loop (triple-nested): 3*3*3 = 27 iterations */
    {
        volatile uint16_t count = 0;
        for (uint8_t i = 0; i < 3; i++) {
            for (uint8_t j = 0; j < 3; j++) {
                for (uint8_t k = 0; k < 3; k++) {
                    count++;
                }
            }
        }
        if (count == 27)
            status |= 2;
    }

    /* Bit 2: nested switch inside switch */
    {
        volatile uint8_t c1 = 1, s1 = 2, c2 = 2, s2 = 3, c3 = 1, s3 = 1;
        uint8_t ok = 1;
        if (nested_switch(c1, s1) != 12) ok = 0;  /* cat1, sub2 */
        if (nested_switch(c2, s2) != 23) ok = 0;  /* cat2, sub3 */
        if (nested_switch(c3, s3) != 11) ok = 0;  /* cat1, sub1 */
        if (ok) status |= 4;
    }

    /* Bit 3: mixed nesting: for containing if containing while containing if */
    {
        volatile uint16_t counter = 0;
        for (uint8_t i = 0; i < 4; i++) {
            if (i % 2 == 0) {
                /* only for even i: 0, 2 */
                uint8_t w = 0;
                while (w < 3) {
                    if (w > 0) {
                        counter++;
                    }
                    w++;
                }
                /* each even i: w=1 and w=2 add to counter -> 2 per even i */
            }
        }
        /* i=0: counter+=2, i=2: counter+=2 -> total = 4 */
        if (counter == 4)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
