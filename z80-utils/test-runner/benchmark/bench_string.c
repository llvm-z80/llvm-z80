/* Benchmark: String operations */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;

uint8_t my_strlen(const uint8_t *s) {
    uint8_t len = 0;
    while (s[len] != 0) len++;
    return len;
}

int8_t my_strcmp(const uint8_t *a, const uint8_t *b) {
    while (*a && *a == *b) { a++; b++; }
    if (*a == *b) return 0;
    return *a < *b ? -1 : 1;
}

void my_strcpy(uint8_t *dst, const uint8_t *src) {
    while (*src) { *dst++ = *src++; }
    *dst = 0;
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

void simple_itoa(uint16_t val, uint8_t *buf) {
    uint8_t i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (val > 0) {
        buf[i++] = '0' + (uint8_t)(val % 10);
        val /= 10;
    }
    buf[i] = 0;
    my_strrev(buf, i);
}

void my_memset(uint8_t *dst, uint8_t val, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) dst[i] = val;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: strlen */
    {
        uint8_t s1[] = "hello";
        uint8_t s2[] = "";
        if (my_strlen(s1) == 5 && my_strlen(s2) == 0)
            status |= 1;
    }

    /* Bit 1: strcmp */
    {
        uint8_t a[] = "abc";
        uint8_t b[] = "abc";
        uint8_t c[] = "abd";
        int8_t r1 = my_strcmp(a, b);
        int8_t r2 = my_strcmp(a, c);
        int8_t r3 = my_strcmp(c, a);
        if (r1 == 0 && r2 < 0 && r3 > 0)
            status |= 2;
    }

    /* Bit 2: strcpy */
    {
        uint8_t src[] = "test";
        uint8_t dst[8];
        my_strcpy(dst, src);
        if (dst[0] == 't' && dst[3] == 't' && dst[4] == 0)
            status |= 4;
    }

    /* Bit 3: string reverse */
    {
        uint8_t s[] = "abcde";
        my_strrev(s, 5);
        if (s[0] == 'e' && s[2] == 'c' && s[4] == 'a')
            status |= 8;
    }

    /* Bit 4: atoi */
    {
        uint8_t s1[] = "12345";
        uint8_t s2[] = "0";
        if (simple_atoi(s1) == 12345 && simple_atoi(s2) == 0)
            status |= 0x10;
    }

    /* Bit 5: itoa roundtrip */
    {
        uint8_t buf[8];
        simple_itoa(6789, buf);
        if (simple_atoi(buf) == 6789)
            status |= 0x20;
    }

    /* Bit 6: memset and verify */
    {
        uint8_t buf[16];
        my_memset(buf, 0xAA, 16);
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 16; i++) {
            if (buf[i] != 0xAA) ok = 0;
        }
        if (ok) status |= 0x40;
    }

    /* Bit 7: String concatenation via copy+append */
    {
        uint8_t buf[16];
        uint8_t a[] = "hel";
        uint8_t b[] = "lo";
        my_strcpy(buf, a);
        uint8_t len = my_strlen(buf);
        my_strcpy(buf + len, b);
        if (my_strlen(buf) == 5 && my_strcmp(buf, (const uint8_t *)"hello") == 0)
            status |= 0x80;
    }

    return status; /* expect 0x00FF */
}
