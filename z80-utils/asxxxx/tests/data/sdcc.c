extern unsigned char tbl[];
extern void ext(void);
unsigned char g = 3;
unsigned char hi(void) { return (unsigned)(tbl + 300) >> 8; }
unsigned char lo(void) { return (unsigned)(tbl + 300) & 0xff; }
void tail(void) { ext(); }
