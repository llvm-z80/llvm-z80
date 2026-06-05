/* Test 24: Multiple return paths - early return, binary search, char classify, find */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* Classify with 5 early returns: neg->0, zero->1, small(1-10)->2, medium(11-100)->3, large(>100)->4 */
uint8_t classify_value(int16_t x) {
    if (x < 0) return 0;
    if (x == 0) return 1;
    if (x <= 10) return 2;
    if (x <= 100) return 3;
    return 4;
}

/* Binary search in sorted array. Returns index if found, -1 if not found */
int16_t binary_search(volatile uint16_t *arr, uint8_t len, uint16_t target) {
    uint8_t lo = 0;
    uint8_t hi = len - 1;
    while (lo <= hi) {
        uint8_t mid = (lo + hi) >> 1;
        if (arr[mid] == target) return (int16_t)mid;
        else if (arr[mid] < target) lo = mid + 1;
        else {
            if (mid == 0) return -1;
            hi = mid - 1;
        }
    }
    return -1;
}

/* Char classification */
uint8_t is_digit(uint8_t c) {
    return (c >= '0' && c <= '9') ? 1 : 0;
}

uint8_t is_upper(uint8_t c) {
    return (c >= 'A' && c <= 'Z') ? 1 : 0;
}

uint8_t is_lower(uint8_t c) {
    return (c >= 'a' && c <= 'z') ? 1 : 0;
}

uint8_t is_space(uint8_t c) {
    return (c == ' ' || c == '\t' || c == '\n') ? 1 : 0;
}

/* Find first matching value in array, or -1 if none */
int16_t find_value(volatile uint16_t *arr, uint8_t len, uint16_t target) {
    for (uint8_t i = 0; i < len; i++) {
        if (arr[i] == target) return (int16_t)i;
    }
    return -1;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: classify with 5 early returns */
    {
        volatile int16_t v1 = -5, v2 = 0, v3 = 7, v4 = 50, v5 = 200;
        uint8_t ok = 1;
        if (classify_value(v1) != 0) ok = 0; /* negative */
        if (classify_value(v2) != 1) ok = 0; /* zero */
        if (classify_value(v3) != 2) ok = 0; /* small */
        if (classify_value(v4) != 3) ok = 0; /* medium */
        if (classify_value(v5) != 4) ok = 0; /* large */
        if (ok) status |= 1;
    }

    /* Bit 1: binary search - found and not found */
    {
        volatile uint16_t sorted[6];
        sorted[0] = 10; sorted[1] = 20; sorted[2] = 30;
        sorted[3] = 50; sorted[4] = 60; sorted[5] = 80;

        int16_t idx1 = binary_search(sorted, 6, 50); /* found at index 3 */
        int16_t idx2 = binary_search(sorted, 6, 35); /* not found -> -1 */
        int16_t idx3 = binary_search(sorted, 6, 10); /* found at index 0 */
        if (idx1 == 3 && idx2 == -1 && idx3 == 0)
            status |= 2;
    }

    /* Bit 2: char classification */
    {
        volatile uint8_t c1 = '5', c2 = 'A', c3 = 'z', c4 = ' ', c5 = 'x';
        uint8_t ok = 1;
        if (is_digit(c1) != 1) ok = 0;
        if (is_upper(c2) != 1) ok = 0;
        if (is_lower(c3) != 1) ok = 0;
        if (is_space(c4) != 1) ok = 0;
        if (is_digit(c5) != 0) ok = 0;
        if (ok) status |= 4;
    }

    /* Bit 3: find_value with multiple exit points */
    {
        volatile uint16_t arr[5];
        arr[0] = 42; arr[1] = 17; arr[2] = 99; arr[3] = 8; arr[4] = 55;

        int16_t r1 = find_value(arr, 5, 99); /* found at index 2 */
        int16_t r2 = find_value(arr, 5, 42); /* found at index 0 */
        int16_t r3 = find_value(arr, 5, 77); /* not found -> -1 */
        if (r1 == 2 && r2 == 0 && r3 == -1)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
