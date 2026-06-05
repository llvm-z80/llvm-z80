/* Test 27: 2D array operations - trace, transpose, matrix multiply */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

int main() {
    uint16_t status = 0;

    /* Bit 0: 3x3 matrix trace and total sum */
    {
        volatile uint16_t m[3][3];
        m[0][0] = 1; m[0][1] = 2; m[0][2] = 3;
        m[1][0] = 4; m[1][1] = 5; m[1][2] = 6;
        m[2][0] = 7; m[2][1] = 8; m[2][2] = 9;
        uint16_t trace = m[0][0] + m[1][1] + m[2][2]; /* 1+5+9=15 */
        uint16_t total = 0;
        for (uint8_t i = 0; i < 3; i++)
            for (uint8_t j = 0; j < 3; j++)
                total += m[i][j];
        /* total = 1+2+...+9 = 45 */
        if (trace == 15 && total == 45) status |= 1;
    }

    /* Bit 1: 3x3 transpose: verify m[i][j]==orig[j][i] for all i,j */
    {
        volatile uint8_t orig[3][3];
        orig[0][0] = 1; orig[0][1] = 2; orig[0][2] = 3;
        orig[1][0] = 4; orig[1][1] = 5; orig[1][2] = 6;
        orig[2][0] = 7; orig[2][1] = 8; orig[2][2] = 9;
        uint8_t t[3][3];
        for (uint8_t i = 0; i < 3; i++)
            for (uint8_t j = 0; j < 3; j++)
                t[j][i] = orig[i][j];
        uint8_t ok = 1;
        for (uint8_t i = 0; i < 3; i++)
            for (uint8_t j = 0; j < 3; j++)
                if (t[i][j] != orig[j][i]) ok = 0;
        if (ok) status |= 2;
    }

    /* Bit 2: 2x2 matrix multiply: {{1,2},{3,4}} * {{5,6},{7,8}} = {{19,22},{43,50}} */
    {
        volatile uint8_t a[2][2];
        volatile uint8_t b[2][2];
        a[0][0] = 1; a[0][1] = 2; a[1][0] = 3; a[1][1] = 4;
        b[0][0] = 5; b[0][1] = 6; b[1][0] = 7; b[1][1] = 8;
        uint16_t r[2][2];
        for (uint8_t i = 0; i < 2; i++)
            for (uint8_t j = 0; j < 2; j++) {
                r[i][j] = 0;
                for (uint8_t k = 0; k < 2; k++)
                    r[i][j] += (uint16_t)a[i][k] * b[k][j];
            }
        if (r[0][0] == 19 && r[0][1] == 22 &&
            r[1][0] == 43 && r[1][1] == 50)
            status |= 4;
    }

    /* Bit 3: identity multiply: A * I == A for 2x2 matrix */
    {
        volatile uint8_t a[2][2];
        a[0][0] = 3; a[0][1] = 7; a[1][0] = 2; a[1][1] = 5;
        uint8_t id[2][2];
        id[0][0] = 1; id[0][1] = 0; id[1][0] = 0; id[1][1] = 1;
        uint16_t r[2][2];
        for (uint8_t i = 0; i < 2; i++)
            for (uint8_t j = 0; j < 2; j++) {
                r[i][j] = 0;
                for (uint8_t k = 0; k < 2; k++)
                    r[i][j] += (uint16_t)a[i][k] * id[k][j];
            }
        if (r[0][0] == 3 && r[0][1] == 7 &&
            r[1][0] == 2 && r[1][1] == 5)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
