typedef unsigned short u16;

extern volatile u16 weak_data;

u16 sdcc_read_weak_data(void) { return weak_data; }
