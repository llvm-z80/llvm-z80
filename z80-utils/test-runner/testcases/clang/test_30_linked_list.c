/* Test 30: Linked list operations - build, count, sum, max, tail */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

struct Node {
    uint16_t data;
    struct Node *next;
};

struct Node pool[5];

void list_build(volatile uint16_t *vals, uint8_t n) {
    uint8_t i;
    for (i = 0; i < n; i++)
        pool[i].data = vals[i];
    for (i = 0; i + 1 < n; i++)
        pool[i].next = &pool[i + 1];
    pool[n - 1].next = (struct Node *)0;
}

uint8_t list_count(struct Node *p) {
    uint8_t c = 0;
    while (p) { c++; p = p->next; }
    return c;
}

uint16_t list_sum(struct Node *p) {
    uint16_t sum = 0;
    while (p) { sum += p->data; p = p->next; }
    return sum;
}

uint16_t list_max(struct Node *p) {
    uint16_t mx = 0;
    while (p) {
        if (p->data > mx) mx = p->data;
        p = p->next;
    }
    return mx;
}

struct Node *list_last(struct Node *p) {
    while (p->next) p = p->next;
    return p;
}

int main() {
    uint16_t status = 0;

    volatile uint16_t vals[5];
    vals[0] = 10; vals[1] = 20; vals[2] = 30; vals[3] = 40; vals[4] = 50;

    list_build(vals, 5);
    struct Node *head = &pool[0];

    /* Bit 0: build linked list of 5 nodes, count==5 */
    if (list_count(head) == 5) status |= 1;

    /* Bit 1: sum all node values == 150 */
    if (list_sum(head) == 150) status |= 2;

    /* Bit 2: find max value == 50 */
    if (list_max(head) == 50) status |= 4;

    /* Bit 3: last node's next==0 (NULL), last node's value==50 */
    {
        struct Node *last = list_last(head);
        if (last->next == (struct Node *)0 && last->data == 50)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
