/* Test 32: Union type punning, nested structs, initializers */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: union type punning: w=0x1234 -> b[0]==0x34, b[1]==0x12 (LE) */
    {
        union {
            uint16_t w;
            uint8_t b[2];
        } u;
        u.w = 0x1234;
        if (u.b[0] == 0x34 && u.b[1] == 0x12)
            status |= 1;
    }

    /* Bit 1: nested struct: Rect{Point min, Point max}; field access */
    {
        typedef struct { int16_t x; int16_t y; } Point;
        typedef struct { Point min; Point max; } Rect;
        Rect r;
        r.min.x = 10; r.min.y = 20;
        r.max.x = 30; r.max.y = 50;
        if (r.min.x == 10 && r.min.y == 20 &&
            r.max.x == 30 && r.max.y == 50)
            status |= 2;
    }

    /* Bit 2: struct initializer and array init */
    {
        typedef struct { int16_t x; int16_t y; } Point;
        Point p = {10, 20};
        volatile int16_t arr[5];
        arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4; arr[4] = 5;
        if (p.x == 10 && p.y == 20 &&
            arr[0] == 1 && arr[2] == 3 && arr[4] == 5)
            status |= 4;
    }

    /* Bit 3: 2D array init: diagonal sum m[0][0]+m[1][1]==6 */
    {
        volatile int16_t m[2][3];
        m[0][0] = 1; m[0][1] = 2; m[0][2] = 3;
        m[1][0] = 4; m[1][1] = 5; m[1][2] = 6;
        int16_t diag = m[0][0] + m[1][1]; /* 1+5=6 */
        if (diag == 6) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
