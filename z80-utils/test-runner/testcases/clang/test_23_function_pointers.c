/* Test 23: Function pointers - basic, as argument, array of fptrs, transform chain */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

int16_t add_one(int16_t x) { return x + 1; }
int16_t double_it(int16_t x) { return x * 2; }
int16_t square(int16_t x) { return x * x; }
int16_t negate(int16_t x) { return -x; }

int16_t apply(int16_t (*f)(int16_t), int16_t x) {
    return f(x);
}

/* Binary operations for array of function pointers */
int16_t op_add(int16_t a, int16_t b) { return a + b; }
int16_t op_sub(int16_t a, int16_t b) { return a - b; }
int16_t op_mul(int16_t a, int16_t b) { return a * b; }
int16_t op_div(int16_t a, int16_t b) { return a / b; }

/* Apply a chain of 3 unary transforms */
int16_t apply_chain(int16_t (*f1)(int16_t), int16_t (*f2)(int16_t), int16_t (*f3)(int16_t), int16_t x) {
    return f3(f2(f1(x)));
}

int main() {
    uint16_t status = 0;

    /* Bit 0: basic function pointer call */
    {
        int16_t (*fptr)(int16_t);
        volatile int16_t x = 5;
        fptr = &add_one;
        int16_t r1 = fptr(x);      /* 6 */
        fptr = &double_it;
        int16_t r2 = fptr(x);      /* 10 */
        if (r1 == 6 && r2 == 10)
            status |= 1;
    }

    /* Bit 1: function pointer as argument */
    {
        volatile int16_t x = 3;
        int16_t r1 = apply(square, x);   /* 9 */
        int16_t r2 = apply(negate, x);   /* -3 */
        if (r1 == 9 && r2 == -3)
            status |= 2;
    }

    /* Bit 2: array of function pointers */
    {
        int16_t (*ops[4])(int16_t, int16_t);
        ops[0] = op_add;
        ops[1] = op_sub;
        ops[2] = op_mul;
        ops[3] = op_div;

        volatile int16_t a = 10, b = 3;
        int16_t r0 = ops[0](a, b);  /* 10+3 = 13 */
        int16_t r1 = ops[1](a, b);  /* 10-3 = 7 */
        int16_t r2 = ops[2](a, b);  /* 10*3 = 30 */
        int16_t r3 = ops[3](a, b);  /* 10/3 = 3 */
        if (r0 == 13 && r1 == 7 && r2 == 30 && r3 == 3)
            status |= 4;
    }

    /* Bit 3: transform chain: (5+1)*2+1 = 13 */
    {
        volatile int16_t x = 5;
        /* chain: add_one -> double_it -> add_one */
        /* (5+1)=6, 6*2=12, 12+1=13 */
        int16_t result = apply_chain(add_one, double_it, add_one, x);
        if (result == 13)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
