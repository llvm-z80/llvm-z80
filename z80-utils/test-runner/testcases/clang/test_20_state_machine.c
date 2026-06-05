/* Test 20: State machine - transitions, event counting, error state, multi-session */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uint32_t;
typedef long int32_t;

/* States: 0=IDLE, 1=COUNTING, 2=DONE, 3=ERROR */
/* Commands: 0=NOP, 1=START, 2=STOP, 3=DATA, 9=INVALID */

struct SM {
    uint8_t state;
    uint16_t data_count;
    uint8_t error_flag;
    uint8_t done_count;
};

void sm_init(struct SM *sm) {
    sm->state = 0;
    sm->data_count = 0;
    sm->error_flag = 0;
    sm->done_count = 0;
}

void sm_step(struct SM *sm, uint8_t cmd) {
    switch (sm->state) {
        case 0: /* IDLE */
            if (cmd == 1) sm->state = 1;      /* START -> COUNTING */
            else if (cmd == 9) sm->state = 3;  /* INVALID -> ERROR */
            break;
        case 1: /* COUNTING */
            if (cmd == 3) {
                sm->data_count++;
            } else if (cmd == 2) {
                sm->done_count++;
                sm->state = 2; /* STOP -> DONE */
            } else if (cmd == 9) {
                sm->state = 3; /* INVALID -> ERROR */
                sm->error_flag = 1;
            }
            break;
        case 2: /* DONE */
            if (cmd == 1) sm->state = 1; /* can restart: START -> COUNTING */
            else sm->state = 0;          /* anything else -> IDLE */
            break;
        case 3: /* ERROR */
            /* stay in error */
            break;
        default:
            break;
    }
}

int main() {
    uint16_t status = 0;

    /* Bit 0: simple state machine IDLE->COUNTING->DONE */
    {
        struct SM sm;
        sm_init(&sm);

        volatile uint8_t seq[6];
        seq[0] = 0; /* NOP - stay IDLE */
        seq[1] = 1; /* START -> COUNTING */
        seq[2] = 3; /* DATA */
        seq[3] = 3; /* DATA */
        seq[4] = 3; /* DATA */
        seq[5] = 2; /* STOP -> DONE */

        for (uint8_t i = 0; i < 6; i++) {
            sm_step(&sm, seq[i]);
        }
        /* Final state should be DONE(2), data_count=3, done_count=1 */
        if (sm.state == 2 && sm.done_count == 1)
            status |= 1;
    }

    /* Bit 1: count specific events during execution */
    {
        struct SM sm;
        sm_init(&sm);

        volatile uint8_t seq[8];
        seq[0] = 1; /* START */
        seq[1] = 3; /* DATA */
        seq[2] = 3; /* DATA */
        seq[3] = 2; /* STOP -> DONE */
        seq[4] = 1; /* START (restart) */
        seq[5] = 3; /* DATA */
        seq[6] = 3; /* DATA */
        seq[7] = 2; /* STOP -> DONE */

        for (uint8_t i = 0; i < 8; i++) {
            sm_step(&sm, seq[i]);
        }
        /* data_count should be 4 total, done_count should be 2 */
        if (sm.data_count == 4 && sm.done_count == 2)
            status |= 2;
    }

    /* Bit 2: error state - inject invalid input */
    {
        struct SM sm;
        sm_init(&sm);

        volatile uint8_t seq[5];
        seq[0] = 1; /* START -> COUNTING */
        seq[1] = 3; /* DATA */
        seq[2] = 9; /* INVALID -> ERROR */
        seq[3] = 3; /* should stay in ERROR */
        seq[4] = 1; /* should stay in ERROR */

        for (uint8_t i = 0; i < 5; i++) {
            sm_step(&sm, seq[i]);
        }
        if (sm.state == 3 && sm.error_flag == 1 && sm.data_count == 1)
            status |= 4;
    }

    /* Bit 3: multiple sessions - reset and run twice, accumulate */
    {
        struct SM sm;
        sm_init(&sm);

        /* Session 1 */
        volatile uint8_t s1[4];
        s1[0] = 1; s1[1] = 3; s1[2] = 3; s1[3] = 2;
        for (uint8_t i = 0; i < 4; i++) sm_step(&sm, s1[i]);
        uint16_t count1 = sm.data_count; /* should be 2 */

        /* Reset state but keep accumulated data_count */
        sm.state = 0;

        /* Session 2 */
        volatile uint8_t s2[5];
        s2[0] = 1; s2[1] = 3; s2[2] = 3; s2[3] = 3; s2[4] = 2;
        for (uint8_t i = 0; i < 5; i++) sm_step(&sm, s2[i]);
        uint16_t count2 = sm.data_count; /* should be 5 (accumulated: 2+3) */

        if (count1 == 2 && count2 == 5 && sm.done_count == 2)
            status |= 8;
    }

    return status; /* expect 0x000F = 15 */
}
