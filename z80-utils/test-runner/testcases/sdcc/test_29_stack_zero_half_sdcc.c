typedef unsigned short u16;
typedef unsigned long u32;

union FB { float f; u16 w[2]; };

u16 sdcc_zh_c0_lo(u32 a) __sdcccall(0) { return (u16)a; }
u16 sdcc_zh_c0_hi(u32 a) __sdcccall(0) { return (u16)(a >> 16); }
u16 sdcc_zh_sc_lo(u32 a) __smallc { return (u16)a; }
u16 sdcc_zh_scc_lo(u32 a) __smallc __z88dk_callee { return (u16)a; }
u16 sdcc_zh_c0_flo(float a) __sdcccall(0) { union FB u; u.f = a; return u.w[0]; }
u16 sdcc_zh_sc_flo(float a) __smallc { union FB u; u.f = a; return u.w[0]; }
