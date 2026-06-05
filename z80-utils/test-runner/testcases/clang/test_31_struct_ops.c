/* Test 31: Struct operations - field access, pass by pointer, arrays, dot product */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;

typedef struct {
    int16_t x;
    int16_t y;
} Point;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
} Vec3;

uint16_t manhattan_distance(Point *a, Point *b) {
    int16_t dx = a->x - b->x;
    int16_t dy = a->y - b->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (uint16_t)(dx + dy);
}

uint16_t vec3_dot(Vec3 *a, Vec3 *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: struct field access: Point{3,4}; p.x==3, p.y==4 */
    {
        Point p;
        p.x = 3;
        p.y = 4;
        if (p.x == 3 && p.y == 4) status |= 1;
    }

    /* Bit 1: struct pass by pointer: manhattan_distance({1,2},{4,6})==7 */
    {
        Point a, b;
        a.x = 1; a.y = 2;
        b.x = 4; b.y = 6;
        /* |4-1| + |6-2| = 3 + 4 = 7 */
        if (manhattan_distance(&a, &b) == 7) status |= 2;
    }

    /* Bit 2: array of structs: 3 Points, sum all x and y values */
    {
        Point pts[3];
        pts[0].x = 1; pts[0].y = 2;
        pts[1].x = 3; pts[1].y = 4;
        pts[2].x = 5; pts[2].y = 6;
        uint16_t sum = 0;
        for (uint8_t i = 0; i < 3; i++)
            sum += pts[i].x + pts[i].y;
        /* (1+2)+(3+4)+(5+6) = 21 */
        if (sum == 21) status |= 4;
    }

    /* Bit 3: Vec3 dot product: {1,2,3}.{4,5,6} = 4+10+18 = 32 */
    {
        Vec3 a, b;
        a.x = 1; a.y = 2; a.z = 3;
        b.x = 4; b.y = 5; b.z = 6;
        if (vec3_dot(&a, &b) == 32) status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
