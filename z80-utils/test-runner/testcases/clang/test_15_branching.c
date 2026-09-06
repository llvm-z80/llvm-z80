/* Test 15: Branching - if-else chains, do-while, break/continue, early return */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* Classify value: <0 returns -1, ==0 returns 0, >0 returns 1 */
int16_t classify(int16_t x) {
    if (x < 0) return -1;
    else if (x == 0) return 0;
    else return 1;
}

/* Find index of first negative in array, or -1 if none */
int16_t find_first_negative(volatile int16_t *arr, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (arr[i] < 0) return (int16_t)i;
    }
    return -1;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: if-else chain classify */
    {
        volatile int16_t v1 = -5;
        volatile int16_t v2 = 0;
        volatile int16_t v3 = 7;
        if (classify(v1) == -1 && classify(v2) == 0 && classify(v3) == 1)
            status |= 1;
    }

    /* Bit 1: do-while sum 1..10 = 55; do-while body runs at least once */
    {
        volatile uint16_t n = 10;
        uint16_t sum = 0;
        uint16_t i = 1;
        do {
            sum += i;
            i++;
        } while (i <= n);
        /* sum = 1+2+...+10 = 55 */

        /* do-while with false condition: body runs once */
        volatile uint8_t once = 0;
        uint8_t count = 0;
        do {
            count++;
        } while (once);
        /* count should be 1 (body ran once even though condition is false) */

        if (sum == 55 && count == 1)
            status |= 2;
    }

    /* Bit 2: break in while (sum until > 100); continue in for (sum odd 1..20) */
    {
        /* break: sum until exceeds 100 */
        volatile uint16_t limit = 100;
        uint16_t sum = 0;
        uint16_t i = 1;
        while (1) {
            sum += i;
            if (sum > limit) break;
            i++;
        }
        /* 1+2+...+13 = 91, +14 = 105 > 100, so sum=105, i=14 */

        /* continue: sum odd numbers 1..20, skip even */
        uint16_t odd_sum = 0;
        for (uint16_t j = 1; j <= 20; j++) {
            if ((j & 1) == 0) continue;
            odd_sum += j;
        }
        /* 1+3+5+7+9+11+13+15+17+19 = 100 */

        if (sum == 105 && i == 14 && odd_sum == 100)
            status |= 4;
    }

    /* Bit 3: early return from function - find first negative */
    {
        volatile int16_t arr1[4];
        arr1[0] = 3; arr1[1] = 1; arr1[2] = -2; arr1[3] = 5;

        volatile int16_t arr2[3];
        arr2[0] = 1; arr2[1] = 2; arr2[2] = 3;

        int16_t idx1 = find_first_negative(arr1, 4);
        int16_t idx2 = find_first_negative(arr2, 3);

        if (idx1 == 2 && idx2 == -1)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
