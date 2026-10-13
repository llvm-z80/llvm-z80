/* Test 74: _Thread_local variables under -femulated-tls.
 *
 * Every access calls __emutls_get_address, which the program provides. This
 * version hands out one copy per variable from a static pool, copying the
 * template or zero-filling the copy on first use. */
/* EXTRA-FLAGS: -femulated-tls */

struct emutls_control {
    unsigned short size, align;
    void *index;
    void *templ;
};

static unsigned char pool[16];
static unsigned short used;
static unsigned short calls;

void *__emutls_get_address(struct emutls_control *c) {
    calls++;
    if (!c->index) {
        unsigned char *p = pool + used;
        const unsigned char *t = c->templ;
        used += c->size;
        for (unsigned short i = 0; i < c->size; i++)
            p[i] = t ? t[i] : 0;
        c->index = p;
    }
    return c->index;
}

static _Thread_local int counter = 3;
static _Thread_local int zero;

int probe(void) {
    counter++;
    return counter == 4;
}

int main() {
    int status = 0;

    if (probe())
        status |= 1;
    zero++;
    if (zero == 1)
        status |= 2;
    if (calls != 0)
        status |= 4;

    return status; /* expect 0x0007 */
}
