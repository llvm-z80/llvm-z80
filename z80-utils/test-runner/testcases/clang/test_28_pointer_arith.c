/* Test 28: Pointer arithmetic - subtraction, increment, array of pointers, double pointer */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

void swap_via_pp(int16_t **pa, int16_t **pb) {
    int16_t tmp = **pa;
    **pa = **pb;
    **pb = tmp;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: pointer subtraction and comparison */
    {
        int16_t arr[5];
        arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40; arr[4] = 50;
        int16_t *p0 = &arr[0];
        int16_t *p3 = &arr[3];
        int16_t diff = p3 - p0;
        if (diff == 3 && &arr[2] > &arr[1]) status |= 1;
    }

    /* Bit 1: pointer increment in loop: *p++ to sum array elements */
    {
        volatile int16_t arr[5];
        arr[0] = 5; arr[1] = 10; arr[2] = 15; arr[3] = 20; arr[4] = 25;
        int16_t *p = (int16_t *)arr;
        int16_t sum = 0;
        uint8_t count = 5;
        while (count--) {
            sum += *p++;
        }
        /* 5+10+15+20+25 = 75 */
        if (sum == 75) status |= 2;
    }

    /* Bit 2: array of pointers: int16_t* ptrs[3] pointing to vars; sum via ptrs */
    {
        volatile int16_t a = 100;
        volatile int16_t b = 200;
        volatile int16_t c = 300;
        int16_t *ptrs[3];
        ptrs[0] = (int16_t *)&a;
        ptrs[1] = (int16_t *)&b;
        ptrs[2] = (int16_t *)&c;
        int16_t sum = 0;
        for (uint8_t i = 0; i < 3; i++)
            sum += *ptrs[i];
        if (sum == 600) status |= 4;
    }

    /* Bit 3: double pointer: swap two i16 values via pointer-to-pointer */
    {
        volatile int16_t x = 42;
        volatile int16_t y = 99;
        int16_t *px = (int16_t *)&x;
        int16_t *py = (int16_t *)&y;
        swap_via_pp(&px, &py);
        if (x == 99 && y == 42) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
