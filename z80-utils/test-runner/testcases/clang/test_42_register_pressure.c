/* Test 42: Register pressure - many simultaneous values, live across calls */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

uint16_t add16(uint16_t a, uint16_t b) { return a + b; }
uint16_t sub16(uint16_t a, uint16_t b) { return a - b; }

int main() {
    uint16_t status = 0;

    /* Bit 0: 6 simultaneous i16 values: compute expression using all 6 */
    {
        volatile uint16_t a = 100, b = 200, c = 300;
        volatile uint16_t d = 400, e = 500, f = 600;
        uint16_t result = a + b + c + d + e + f;
        /* 100+200+300+400+500+600 = 2100 */
        if (result == 2100) status |= 1;
    }

    /* Bit 1: 7 simultaneous i8 values: compute chain using all */
    {
        volatile uint8_t a = 10, b = 20, c = 30, d = 40;
        volatile uint8_t e = 50, f = 60, g = 70;
        uint16_t result = (uint16_t)a + b + c + d + e + f + g;
        /* 10+20+30+40+50+60+70 = 280 */
        if (result == 280) status |= 2;
    }

    /* Bit 2: values live across function call: a,b,c,d live; call func(); use after */
    {
        volatile uint16_t a = 100, b = 200, c = 300, d = 400;
        uint16_t r1 = add16(a, b); /* 300 */
        /* a, b, c, d, r1 all live here */
        uint16_t r2 = sub16(d, a); /* 300 */
        if (r1 == c && r2 == c) status |= 4;
    }

    /* Bit 3: comma operator chain: (a=1, b=2, c=a+b, c*2)==6; expression as array index */
    {
        volatile uint16_t a, b, c;
        uint16_t r = (a = 1, b = 2, c = a + b, c * 2);
        /* Also: complex expression as array index */
        uint8_t arr[5];
        arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40; arr[4] = 50;
        volatile uint8_t idx = 3;
        uint8_t val = arr[(idx > 2) ? idx - 1 : idx + 1]; /* arr[2]=30 */
        if (r == 6 && val == 30) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
