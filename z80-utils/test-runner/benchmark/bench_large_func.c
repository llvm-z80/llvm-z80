/* Benchmark: Large functions with branches spanning >127 bytes.
 * Tests JR→JP branch relaxation cleanup effectiveness.
 * expect 0x000F
 */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

volatile uint8_t sink;

/* --- State machine interpreter (large switch) --- */
uint16_t state_machine(const uint8_t *prog, uint8_t len) {
    uint16_t acc = 0;
    uint8_t reg = 0;
    for (uint8_t pc = 0; pc < len; pc++) {
        uint8_t op = prog[pc];
        switch (op & 0x0F) {
        case 0: acc += reg; break;
        case 1: acc -= reg; break;
        case 2: acc ^= reg; break;
        case 3: acc |= reg; break;
        case 4: acc &= 0xFF00 | reg; break;
        case 5: reg = (uint8_t)acc; break;
        case 6: acc <<= 1; break;
        case 7: acc >>= 1; break;
        case 8: reg += op >> 4; break;
        case 9: reg -= op >> 4; break;
        case 10: acc += (uint16_t)(op >> 4) << 8; break;
        case 11: acc = (acc << 8) | (acc >> 8); break;
        case 12: if (reg == 0) acc = 0; break;
        case 13: reg = ~reg; break;
        case 14: acc++; break;
        case 15: acc--; break;
        }
    }
    return acc;
}

/* --- Multi-field struct processor (many conditionals in sequence) --- */
typedef struct {
    uint8_t type;
    uint8_t flags;
    uint8_t val_a;
    uint8_t val_b;
} Record;

uint16_t process_records(const Record *recs, uint8_t count) {
    uint16_t result = 0;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t t = recs[i].type;
        uint8_t f = recs[i].flags;
        uint8_t a = recs[i].val_a;
        uint8_t b = recs[i].val_b;

        if (t == 0) {
            if (f & 1) result += a;
            if (f & 2) result += b;
            if (f & 4) result += (uint16_t)a << 8;
            if (f & 8) result -= b;
        } else if (t == 1) {
            if (f & 1) result ^= a;
            if (f & 2) result ^= b;
            if (f & 4) result |= a;
            if (f & 8) result &= 0xFF00 | b;
        } else if (t == 2) {
            result += (uint16_t)a * b;
        } else if (t == 3) {
            if (a > b)
                result += a - b;
            else
                result += b - a;
        } else {
            result += t;
        }

        /* Extra operations to enlarge the function body */
        sink = (uint8_t)result;
        sink = (uint8_t)(result >> 8);
        sink = a ^ b;
        sink = f;
    }
    return result;
}

/* --- Unrolled computation (guaranteed large basic blocks) --- */
uint16_t unrolled_hash(const uint8_t *data, uint8_t len) {
    uint16_t h = 0x1234;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t v = data[i];
        /* 16 rounds of mixing — each round ~8 bytes of code,
         * so the loop body exceeds 127 bytes. */
        h ^= v; h = (h << 1) | (h >> 15);
        h += v; h ^= 0xA5;
        h ^= v; h = (h << 1) | (h >> 15);
        h -= v; h ^= 0x5A;
        h ^= v; h = (h << 2) | (h >> 14);
        h += v; h ^= 0x96;
        h ^= v; h = (h << 3) | (h >> 13);
        h -= v; h ^= 0x69;
        h ^= v; h = (h << 1) | (h >> 15);
        h += v; h ^= 0xC3;
        h ^= v; h = (h << 1) | (h >> 15);
        h -= v; h ^= 0x3C;
        h ^= v; h = (h << 2) | (h >> 14);
        h += v; h ^= 0xF0;
        h ^= v; h = (h << 3) | (h >> 13);
        h -= v; h ^= 0x0F;
    }
    return h;
}

/* --- Large conditional chain (many if-else, each with stores) --- */
uint16_t classify(uint8_t x) {
    uint16_t r = 0;
    if (x < 16) {
        sink = 0; sink = x; sink = 1; sink = x;
        sink = 2; sink = x; sink = 3; sink = x;
        sink = 4; sink = x; sink = 5; sink = x;
        sink = 6; sink = x; sink = 7; sink = x;
        r = x * 3;
    } else if (x < 32) {
        sink = 10; sink = x; sink = 11; sink = x;
        sink = 12; sink = x; sink = 13; sink = x;
        sink = 14; sink = x; sink = 15; sink = x;
        sink = 16; sink = x; sink = 17; sink = x;
        r = x * 5;
    } else if (x < 64) {
        sink = 20; sink = x; sink = 21; sink = x;
        sink = 22; sink = x; sink = 23; sink = x;
        sink = 24; sink = x; sink = 25; sink = x;
        sink = 26; sink = x; sink = 27; sink = x;
        r = x * 7;
    } else if (x < 128) {
        sink = 30; sink = x; sink = 31; sink = x;
        sink = 32; sink = x; sink = 33; sink = x;
        sink = 34; sink = x; sink = 35; sink = x;
        sink = 36; sink = x; sink = 37; sink = x;
        r = x * 11;
    } else {
        sink = 40; sink = x; sink = 41; sink = x;
        sink = 42; sink = x; sink = 43; sink = x;
        sink = 44; sink = x; sink = 45; sink = x;
        sink = 46; sink = x; sink = 47; sink = x;
        r = x * 13;
    }
    return r;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: state machine */
    {
        uint8_t prog[] = {
            0x18, /* case 8: reg += 1 */
            0x28, /* case 8: reg += 2 → reg=3 */
            0x00, /* case 0: acc += reg → acc=3 */
            0x06, /* case 6: acc <<= 1 → acc=6 */
            0x0E, /* case 14: acc++ → acc=7 */
        };
        uint16_t r = state_machine(prog, 5);
        if (r == 7) status |= 1;
    }

    /* Bit 1: record processor */
    {
        Record recs[3];
        recs[0].type = 0; recs[0].flags = 3;
        recs[0].val_a = 10; recs[0].val_b = 20;
        recs[1].type = 1; recs[1].flags = 1;
        recs[1].val_a = 0x05; recs[1].val_b = 0;
        recs[2].type = 3; recs[2].flags = 0;
        recs[2].val_a = 50; recs[2].val_b = 30;
        /* rec0: result = 10 + 20 = 30 = 0x1E
         * rec1: result = 0x1E ^ 0x05 = 0x1B
         * rec2: result = 0x1B + (50-30) = 0x1B + 20 = 0x2F */
        uint16_t r = process_records(recs, 3);
        if (r == 0x2F) status |= 2;
    }

    /* Bit 2: unrolled hash */
    {
        uint8_t data[3];
        data[0] = 'A'; data[1] = 'B'; data[2] = 'C';
        uint16_t h = unrolled_hash(data, 3);
        /* Verify hash is deterministic and non-trivial */
        uint16_t h2 = unrolled_hash(data, 3);
        if (h == h2 && h != 0 && h != 0x1234) status |= 4;
    }

    /* Bit 3: classify */
    {
        uint16_t r1 = classify(10);  /* <16: 10*3 = 30 */
        uint16_t r2 = classify(20);  /* <32: 20*5 = 100 */
        uint16_t r3 = classify(200); /* >=128: 200*13 = 2600 */
        if (r1 == 30 && r2 == 100 && r3 == 2600) status |= 8;
    }

    return status; /* expect 0x000F */
}
