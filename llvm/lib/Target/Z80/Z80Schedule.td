//===-- Z80Schedule.td - Z80 and SM83 instruction timing ---*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Z80 times: https://clrhome.org/table/
// SM83 times: https://gbdev.io/gb-opcodes/optables/
//
//===----------------------------------------------------------------------===//

def Z80Model : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0;
  let CompleteModel = 1;
  let UnsupportedFeatures = [HasSM83];
}

def SM83Model : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0;
  let CompleteModel = 1;
  let UnsupportedFeatures = [HasZ80];
}

//===----------------------------------------------------------------------===//
// Z80
//===----------------------------------------------------------------------===//

let SchedModel = Z80Model in {

def Z80CPU : ProcResource<1>;

class Z80Write<int Cycles> : SchedWriteRes<[Z80CPU]> {
  let Latency = Cycles;
  let ReleaseAtCycles = [Cycles];
}

def : InstRW<[Z80Write<4>], (instrs ADC_A_r, ADD_A_r, AND_r, CCF, CPL, CP_r,
  DAA, DEC_r, DI, EI, EXX, EX_AF_AF, EX_DE_HL, HALT, INC_r, JP_HLind, LD_r_r,
  NOP, OR_r, RLA, RLCA, RRA, RRCA, SBC_A_r, SCF, SUB_r, XOR_r)>;
def : InstRW<[Z80Write<6>], (instrs DEC_SP, DEC_rr, INC_SP, INC_rr, LD_SP_HL)>;
def : InstRW<[Z80Write<7>], (instrs ADC_A_HLind, ADC_A_n, ADD_A_HLind, ADD_A_n,
  AND_HLind, AND_n, CP_HLind, CP_n, LD_A_BCind, LD_A_DEind, LD_BCind_A,
  LD_DEind_A, LD_HLind_r, LD_r_HLind, LD_r_n, OR_HLind, OR_n, SBC_A_HLind,
  SBC_A_n, SUB_HLind, SUB_n, XOR_HLind, XOR_n)>;
def : InstRW<[Z80Write<8>], (instrs BIT_b_r, IM_0, IM_1, IM_2, JP_IXind,
  JP_IYind, NEG, RES_b_r, RLC_r, RL_r, RRC_r, RR_r, SET_b_r, SLA_r, SRA_r,
  SRL_r)>;
def : InstRW<[Z80Write<9>], (instrs LD_A_I, LD_A_R, LD_I_A, LD_R_A)>;
def : InstRW<[Z80Write<10>], (instrs DEC_IX, DEC_IY, INC_IX, INC_IY, JP_C_nn,
  JP_M_nn, JP_NC_nn, JP_NZ_nn, JP_PE_nn, JP_PO_nn, JP_P_nn, JP_Z_nn, JP_nn,
  LD_HLind_n, LD_SP_IX, LD_SP_IY, LD_SP_nn, LD_rr_nn, POP_AF, POP_BC, POP_DE,
  POP_HL, RET)>;
def : InstRW<[Z80Write<11>], (instrs ADD_HL_HL, ADD_HL_SP, ADD_HL_rr, DEC_HLind,
  INC_HLind, IN_A_n, OUT_n_A, PUSH_AF, PUSH_BC, PUSH_DE, PUSH_HL, RET_C, RET_M,
  RET_NC, RET_NZ, RET_P, RET_PE, RET_PO, RET_Z, RST_00, RST_08, RST_10, RST_18,
  RST_20, RST_28, RST_30, RST_38)>;
def : InstRW<[Z80Write<12>], (instrs BIT_0_HLind, BIT_1_HLind, BIT_2_HLind,
  BIT_3_HLind, BIT_4_HLind, BIT_5_HLind, BIT_6_HLind, BIT_7_HLind, IN_A_C,
  IN_B_C, IN_C_C, IN_D_C, IN_E_C, IN_H_C, IN_L_C, JR_C_e, JR_NC_e, JR_NZ_e,
  JR_Z_e, JR_e, OUT_C_A, OUT_C_B, OUT_C_C, OUT_C_D, OUT_C_E, OUT_C_H, OUT_C_L)>;
def : InstRW<[Z80Write<13>], (instrs DJNZ_e, LD_A_nnind, LD_nnind_A)>;
def : InstRW<[Z80Write<14>], (instrs LD_IX_nn, LD_IY_nn, POP_IX, POP_IY, RETI,
  RETN)>;
def : InstRW<[Z80Write<15>], (instrs ADC_HL_SP, ADC_HL_rr, ADD_IX_BC, ADD_IX_DE,
  ADD_IX_IX, ADD_IX_SP, ADD_IY_BC, ADD_IY_DE, ADD_IY_IY, ADD_IY_SP, PUSH_IX,
  PUSH_IY, RES_0_HLind, RES_1_HLind, RES_2_HLind, RES_3_HLind, RES_4_HLind,
  RES_5_HLind, RES_6_HLind, RES_7_HLind, RLC_HLind, RL_HLind, RRC_HLind,
  RR_HLind, SBC_HL_SP, SBC_HL_rr, SET_0_HLind, SET_1_HLind, SET_2_HLind,
  SET_3_HLind, SET_4_HLind, SET_5_HLind, SET_6_HLind, SET_7_HLind, SLA_HLind,
  SRA_HLind, SRL_HLind)>;
def : InstRW<[Z80Write<16>], (instrs CPD, CPI, IND, INI, LDD, LDI, LD_HL_nnind,
  LD_nnind_HL, OUTD, OUTI)>;
def : InstRW<[Z80Write<17>], (instrs CALL_C_nn, CALL_M_nn, CALL_NC_nn,
  CALL_NZ_nn, CALL_PE_nn, CALL_PO_nn, CALL_P_nn, CALL_Z_nn, CALL_nn)>;
def : InstRW<[Z80Write<18>], (instrs RLD, RRD)>;
def : InstRW<[Z80Write<19>], (instrs ADC_A_IXd, ADC_A_IYd, ADD_A_IXd, ADD_A_IYd,
  AND_IXd, AND_IYd, CP_IXd, CP_IYd, EX_SPind_HL, LD_IXd_n, LD_IXd_r, LD_IYd_n,
  LD_IYd_r, LD_r_IXd, LD_r_IYd, OR_IXd, OR_IYd, SBC_A_IXd, SBC_A_IYd, SUB_IXd,
  SUB_IYd, XOR_IXd, XOR_IYd)>;
def : InstRW<[Z80Write<20>], (instrs BIT_0_IXd, BIT_0_IYd, BIT_1_IXd, BIT_1_IYd,
  BIT_2_IXd, BIT_2_IYd, BIT_3_IXd, BIT_3_IYd, BIT_4_IXd, BIT_4_IYd, BIT_5_IXd,
  BIT_5_IYd, BIT_6_IXd, BIT_6_IYd, BIT_7_IXd, BIT_7_IYd, LD_BC_nnind,
  LD_DE_nnind, LD_SP_nnind, LD_nnind_BC, LD_nnind_DE, LD_nnind_SP)>;
def : InstRW<[Z80Write<21>], (instrs CPDR, CPIR, INDR, INIR, LDDR, LDIR, OTDR,
  OTIR)>;
def : InstRW<[Z80Write<23>], (instrs DEC_IXd, DEC_IYd, EX_SPind_IX, EX_SPind_IY,
  INC_IXd, INC_IYd, RES_0_IXd, RES_0_IYd, RES_1_IXd, RES_1_IYd, RES_2_IXd,
  RES_2_IYd, RES_3_IXd, RES_3_IYd, RES_4_IXd, RES_4_IYd, RES_5_IXd, RES_5_IYd,
  RES_6_IXd, RES_6_IYd, RES_7_IXd, RES_7_IYd, RLC_IXd, RLC_IYd, RL_IXd, RL_IYd,
  RRC_IXd, RRC_IYd, RR_IXd, RR_IYd, SET_0_IXd, SET_0_IYd, SET_1_IXd, SET_1_IYd,
  SET_2_IXd, SET_2_IYd, SET_3_IXd, SET_3_IYd, SET_4_IXd, SET_4_IYd, SET_5_IXd,
  SET_5_IYd, SET_6_IXd, SET_6_IYd, SET_7_IXd, SET_7_IYd, SLA_IXd, SLA_IYd,
  SRA_IXd, SRA_IYd, SRL_IXd, SRL_IYd)>;

// A DD or FD prefix in front of HALT adds the 4 T-cycles of the prefix.
def : InstRW<[Z80Write<8>], (instrs HALT_DD, HALT_FD)>;

def : InstRW<[], (instrs COPY)>;

} // SchedModel = Z80Model

//===----------------------------------------------------------------------===//
// SM83
//===----------------------------------------------------------------------===//

let SchedModel = SM83Model in {

def SM83CPU : ProcResource<1>;

class SM83Write<int Cycles> : SchedWriteRes<[SM83CPU]> {
  let Latency = Cycles;
  let ReleaseAtCycles = [Cycles];
}

def : InstRW<[SM83Write<4>], (instrs ADC_A_r, ADD_A_r, AND_r, CCF, CPL, CP_r,
  DAA, DEC_r, DI, EI, HALT, INC_r, JP_HLind, LD_r_r, NOP, OR_r, RLA, RLCA, RRA,
  RRCA, SBC_A_r, SCF, SUB_r, XOR_r)>;
def : InstRW<[SM83Write<8>], (instrs ADC_A_HLind, ADC_A_n, ADD_A_HLind, ADD_A_n,
  ADD_HL_HL, ADD_HL_SP, ADD_HL_rr, AND_HLind, AND_n, BIT_b_r, CP_HLind, CP_n,
  DEC_SP, DEC_rr, INC_SP, INC_rr, LD_A_BCind, LD_A_DEind, LD_A_HLD, LD_A_HLI,
  LD_BCind_A, LD_DEind_A, LD_HLD_A, LD_HLI_A, LD_HLind_r, LD_SP_HL, LD_r_HLind,
  LD_r_n, OR_HLind, OR_n, RES_b_r, RLC_r, RL_r, RRC_r, RR_r, SBC_A_HLind,
  SBC_A_n, SET_b_r, SLA_r, SM83_LDH_A_Cind, SM83_LDH_Cind_A, SRA_r, SRL_r,
  SUB_HLind, SUB_n, SWAP_A, XOR_HLind, XOR_n)>;
def : InstRW<[SM83Write<12>], (instrs BIT_0_HLind, BIT_1_HLind, BIT_2_HLind,
  BIT_3_HLind, BIT_4_HLind, BIT_5_HLind, BIT_6_HLind, BIT_7_HLind, DEC_HLind,
  INC_HLind, JR_C_e, JR_NC_e, JR_NZ_e, JR_Z_e, JR_e, LDHL_SP_e, LD_HLind_n,
  LD_SP_nn, LD_rr_nn, POP_AF, POP_BC, POP_DE, POP_HL, SM83_LDH_A_nind,
  SM83_LDH_nind_A)>;
def : InstRW<[SM83Write<16>], (instrs ADD_SP_e, JP_C_nn, JP_NC_nn, JP_NZ_nn,
  JP_Z_nn, JP_nn, PUSH_AF, PUSH_BC, PUSH_DE, PUSH_HL, RES_0_HLind, RES_1_HLind,
  RES_2_HLind, RES_3_HLind, RES_4_HLind, RES_5_HLind, RES_6_HLind, RES_7_HLind,
  RET, RLC_HLind, RL_HLind, RRC_HLind, RR_HLind, RST_00, RST_08, RST_10, RST_18,
  RST_20, RST_28, RST_30, RST_38, SET_0_HLind, SET_1_HLind, SET_2_HLind,
  SET_3_HLind, SET_4_HLind, SET_5_HLind, SET_6_HLind, SET_7_HLind, SLA_HLind,
  SM83_LD_A_nnind, SM83_LD_nnind_A, SM83_RETI, SRA_HLind, SRL_HLind)>;
def : InstRW<[SM83Write<20>], (instrs RET_C, RET_NC, RET_NZ, RET_Z)>;
def : InstRW<[SM83Write<24>], (instrs CALL_C_nn, CALL_NC_nn, CALL_NZ_nn,
  CALL_Z_nn, CALL_nn)>;

def : InstRW<[], (instrs COPY)>;

} // SchedModel = SM83Model
