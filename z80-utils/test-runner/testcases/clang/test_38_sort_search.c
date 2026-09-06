/* Test 38: Sort and search - bubble sort, insertion sort, binary search */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: bubble sort: {5,3,8,1,9,2} -> {1,2,3,5,8,9}; verify sorted */
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

    /* Bit 1: insertion sort: {4,2,7,1,3} -> {1,2,3,4,7}; verify sorted */
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

    /* Bit 2: binary search found: sorted {1,3,5,7,9,11}, search 7 -> index 3 */
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

    /* Bit 3: binary search not found: search 6 -> -1; search 0 -> -1 */
    {
        volatile uint16_t arr[6];
        arr[0] = 1; arr[1] = 3; arr[2] = 5;
        arr[3] = 7; arr[4] = 9; arr[5] = 11;

        /* Search for 6 */
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

        /* Search for 0 */
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

    return status; /* expect 0x000F = 15 */
}
