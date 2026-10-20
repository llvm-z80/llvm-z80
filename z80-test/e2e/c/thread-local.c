// RUN: %cc %s -o %t
// RUN: %run --expect-result=0x0003 %t

/* An initialized and a zero-initialized _Thread_local variable, each read
 * back after an update. */

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

    return status;
}
