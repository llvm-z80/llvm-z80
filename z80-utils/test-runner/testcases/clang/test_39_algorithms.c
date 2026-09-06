/* Test 39: Algorithms - sieve, Collatz, modpow, isqrt */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

uint16_t collatz_steps(uint16_t n) {
    uint16_t steps = 0;
    while (n != 1) {
        if (n & 1)
            n = 3 * n + 1;
        else
            n = n >> 1;
        steps++;
    }
    return steps;
}

uint16_t modpow(uint16_t base, uint16_t exp, uint16_t mod) {
    uint16_t result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1)
            result = (uint16_t)((uint32_t)result * base % mod);
        exp >>= 1;
        base = (uint16_t)((uint32_t)base * base % mod);
    }
    return result;
}

uint16_t isqrt(uint16_t n) {
    if (n < 2) return n;
    uint16_t x = n;
    uint16_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

int main() {
    uint16_t status = 0;

    /* Bit 0: Sieve of Eratosthenes: count primes up to 30 -> 10 */
    {
        volatile uint8_t sieve[31];
        for (uint8_t i = 0; i <= 30; i++) sieve[i] = 1;
        sieve[0] = 0; sieve[1] = 0;
        for (uint8_t i = 2; i <= 5; i++) {
            if (sieve[i]) {
                for (uint8_t j = i + i; j <= 30; j += i)
                    sieve[j] = 0;
            }
        }
        uint8_t count = 0;
        for (uint8_t i = 2; i <= 30; i++)
            if (sieve[i]) count++;
        if (count == 10) status |= 1;
    }

    /* Bit 1: Collatz steps: steps(27)==111; steps(1)==0 */
    {
        if (collatz_steps(27) == 111 && collatz_steps(1) == 0)
            status |= 2;
    }

    /* Bit 2: modpow(2, 10, 1000) == 24; modpow(3, 5, 100) == 43 */
    {
        if (modpow(2, 10, 1000) == 24 && modpow(3, 5, 100) == 43)
            status |= 4;
    }

    /* Bit 3: integer sqrt: isqrt(100)==10, isqrt(99)==9, isqrt(1)==1 */
    {
        if (isqrt(100) == 10 && isqrt(99) == 9 && isqrt(1) == 1)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
