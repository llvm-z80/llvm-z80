/* Test 73: _Thread_local variables.
 *
 * A program on this target is a single thread, so a thread-local variable is
 * an ordinary global. Both an initialized and a zero-initialized variable are
 * read back after an update. */

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

    return status; /* expect 0x0003 */
}
