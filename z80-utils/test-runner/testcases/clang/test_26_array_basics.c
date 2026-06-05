/* Test 26: Array basics - sum, max, copy, reverse, histogram */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: array sum and max: arr[6]={3,1,4,1,5,9}; sum==23, max==9 */
    {
        volatile uint16_t arr[6];
        arr[0] = 3; arr[1] = 1; arr[2] = 4;
        arr[3] = 1; arr[4] = 5; arr[5] = 9;
        uint16_t sum = 0;
        uint16_t max = 0;
        for (uint8_t i = 0; i < 6; i++) {
            sum += arr[i];
            if (arr[i] > max) max = arr[i];
        }
        if (sum == 23 && max == 9) status |= 1;
    }

    /* Bit 1: array copy and verify: copy 4 elements from src to dst */
    {
        volatile uint16_t src[4];
        volatile uint16_t dst[4];
        src[0] = 10; src[1] = 20; src[2] = 30; src[3] = 40;
        for (uint8_t i = 0; i < 4; i++)
            dst[i] = src[i];
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 4; i++)
            if (dst[i] != src[i]) ok = 0;
        if (ok) status |= 2;
    }

    /* Bit 2: array reverse in-place: {1,2,3,4,5} -> {5,4,3,2,1} */
    {
        volatile uint16_t arr[5];
        arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4; arr[4] = 5;
        for (uint8_t i = 0; i < 2; i++) {
            uint16_t tmp = arr[i];
            arr[i] = arr[4 - i];
            arr[4 - i] = tmp;
        }
        if (arr[0] == 5 && arr[1] == 4 && arr[2] == 3 &&
            arr[3] == 2 && arr[4] == 1)
            status |= 4;
    }

    /* Bit 3: histogram: count frequency of values 0-3 in array */
    {
        volatile uint8_t data[8];
        data[0] = 0; data[1] = 1; data[2] = 2; data[3] = 1;
        data[4] = 0; data[5] = 3; data[6] = 1; data[7] = 2;
        uint8_t hist[4];
        hist[0] = 0; hist[1] = 0; hist[2] = 0; hist[3] = 0;
        for (uint8_t i = 0; i < 8; i++)
            hist[data[i]]++;
        if (hist[0] == 2 && hist[1] == 3 && hist[2] == 2 && hist[3] == 1)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
