/* Test 33: String operations - strlen, strcmp, reverse, atoi */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;

uint8_t my_strlen(const uint8_t *s) {
    uint8_t len = 0;
    while (s[len] != 0) len++;
    return len;
}

/* Returns 0 if equal, <0 if a<b, >0 if a>b (simplified: 1 for <, 2 for >) */
int8_t my_strcmp(const uint8_t *a, const uint8_t *b) {
    while (*a && *a == *b) { a++; b++; }
    if (*a == *b) return 0;
    return *a < *b ? -1 : 1;
}

void my_strrev(uint8_t *s, uint8_t len) {
    for (uint8_t i = 0; i < len / 2; i++) {
        uint8_t t = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = t;
    }
}

uint16_t simple_atoi(const uint8_t *s) {
    uint16_t result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: my_strlen: strlen("hello")==5, strlen("")==0 */
    {
        uint8_t s1[] = "hello";
        uint8_t s2[] = "";
        if (my_strlen(s1) == 5 && my_strlen(s2) == 0)
            status |= 1;
    }

    /* Bit 1: my_strcmp: equal, less, greater */
    {
        uint8_t a[] = "abc";
        uint8_t b[] = "abc";
        uint8_t c[] = "abd";
        int8_t r1 = my_strcmp(a, b);   /* equal: 0 */
        int8_t r2 = my_strcmp(a, c);   /* "abc" < "abd": -1 */
        int8_t r3 = my_strcmp(c, a);   /* "abd" > "abc": 1 */
        if (r1 == 0 && r2 < 0 && r3 > 0)
            status |= 2;
    }

    /* Bit 2: string reverse: reverse "abcd" -> "dcba" */
    {
        uint8_t s[] = "abcd";
        my_strrev(s, 4);
        if (s[0] == 'd' && s[1] == 'c' && s[2] == 'b' && s[3] == 'a')
            status |= 4;
    }

    /* Bit 3: simple_atoi: atoi("12345")==12345, atoi("0")==0 */
    {
        uint8_t s1[] = "12345";
        uint8_t s2[] = "0";
        if (simple_atoi(s1) == 12345 && simple_atoi(s2) == 0)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
