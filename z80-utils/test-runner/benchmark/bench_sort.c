/* Benchmark: Sorting and searching */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: Bubble sort {5,3,8,1,9,2} -> {1,2,3,5,8,9} */
    {
        volatile uint16_t arr[6];
        arr[0] = 5; arr[1] = 3; arr[2] = 8;
        arr[3] = 1; arr[4] = 9; arr[5] = 2;
        for (uint8_t i = 0; i < 5; i++) {
            for (uint8_t j = 0; j < 5 - i; j++) {
                if (arr[j] > arr[j + 1]) {
                    uint16_t tmp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = tmp;
                }
            }
        }
        uint8_t sorted = 1;
        for (uint8_t i = 0; i < 5; i++)
            if (arr[i] > arr[i + 1]) sorted = 0;
        if (sorted && arr[0] == 1 && arr[5] == 9) status |= 1;
    }

    /* Bit 1: Insertion sort {4,2,7,1,3} -> {1,2,3,4,7} */
    {
        volatile uint16_t arr[5];
        arr[0] = 4; arr[1] = 2; arr[2] = 7; arr[3] = 1; arr[4] = 3;
        for (uint8_t i = 1; i < 5; i++) {
            uint16_t key = arr[i];
            uint8_t j = i;
            while (j > 0 && arr[j - 1] > key) {
                arr[j] = arr[j - 1];
                j--;
            }
            arr[j] = key;
        }
        uint8_t sorted = 1;
        for (uint8_t i = 0; i < 4; i++)
            if (arr[i] > arr[i + 1]) sorted = 0;
        if (sorted && arr[0] == 1 && arr[4] == 7) status |= 2;
    }

    /* Bit 2: Binary search found (7 in {1,3,5,7,9,11} -> index 3) */
    {
        volatile uint16_t arr[6];
        arr[0] = 1; arr[1] = 3; arr[2] = 5;
        arr[3] = 7; arr[4] = 9; arr[5] = 11;
        uint16_t target = 7;
        uint8_t lo = 0, hi = 5;
        int16_t found = -1;
        while (lo <= hi) {
            uint8_t mid = (lo + hi) >> 1;
            if (arr[mid] == target) { found = (int16_t)mid; break; }
            else if (arr[mid] < target) lo = mid + 1;
            else {
                if (mid == 0) break;
                hi = mid - 1;
            }
        }
        if (found == 3) status |= 4;
    }

    /* Bit 3: Binary search not found (6 and 0 not in array) */
    {
        volatile uint16_t arr[6];
        arr[0] = 1; arr[1] = 3; arr[2] = 5;
        arr[3] = 7; arr[4] = 9; arr[5] = 11;

        uint16_t target = 6;
        uint8_t lo = 0, hi = 5;
        int16_t found1 = -1;
        while (lo <= hi) {
            uint8_t mid = (lo + hi) >> 1;
            if (arr[mid] == target) { found1 = (int16_t)mid; break; }
            else if (arr[mid] < target) lo = mid + 1;
            else {
                if (mid == 0) break;
                hi = mid - 1;
            }
        }

        target = 0;
        lo = 0; hi = 5;
        int16_t found2 = -1;
        while (lo <= hi) {
            uint8_t mid = (lo + hi) >> 1;
            if (arr[mid] == target) { found2 = (int16_t)mid; break; }
            else if (arr[mid] < target) lo = mid + 1;
            else {
                if (mid == 0) break;
                hi = mid - 1;
            }
        }

        if (found1 == -1 && found2 == -1) status |= 8;
    }

    /* Bit 4: Selection sort on i8 array {9,4,7,2,5} -> {2,4,5,7,9} */
    {
        volatile uint8_t arr[5];
        arr[0] = 9; arr[1] = 4; arr[2] = 7; arr[3] = 2; arr[4] = 5;
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t min_idx = i;
            for (uint8_t j = i + 1; j < 5; j++) {
                if (arr[j] < arr[min_idx]) min_idx = j;
            }
            if (min_idx != i) {
                uint8_t tmp = arr[i];
                arr[i] = arr[min_idx];
                arr[min_idx] = tmp;
            }
        }
        if (arr[0] == 2 && arr[4] == 9) status |= 0x10;
    }

    /* Bit 5: Linear search: find max and min in array */
    {
        volatile uint16_t arr[8];
        arr[0] = 42; arr[1] = 17; arr[2] = 95; arr[3] = 3;
        arr[4] = 88; arr[5] = 61; arr[6] = 7;  arr[7] = 54;
        uint16_t max = arr[0], min = arr[0];
        for (uint8_t i = 1; i < 8; i++) {
            if (arr[i] > max) max = arr[i];
            if (arr[i] < min) min = arr[i];
        }
        if (max == 95 && min == 3) status |= 0x20;
    }

    /* Bit 6: Count elements matching condition */
    {
        volatile uint8_t arr[10];
        arr[0] = 1; arr[1] = 5; arr[2] = 3; arr[3] = 8; arr[4] = 2;
        arr[5] = 9; arr[6] = 4; arr[7] = 7; arr[8] = 6; arr[9] = 10;
        uint8_t count_even = 0, count_gt5 = 0;
        for (uint8_t i = 0; i < 10; i++) {
            if ((arr[i] & 1) == 0) count_even++;
            if (arr[i] > 5) count_gt5++;
        }
        /* evens: 8,2,4,6,10 = 5; gt5: 8,9,7,6,10 = 5 */
        if (count_even == 5 && count_gt5 == 5) status |= 0x40;
    }

    /* Bit 7: Sum sorted array and verify partial sums */
    {
        volatile uint16_t arr[5];
        arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40; arr[4] = 50;
        uint16_t prefix = 0;
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 5; i++) {
            prefix += arr[i];
        }
        /* 10+20+30+40+50 = 150 */
        if (prefix == 150) status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
