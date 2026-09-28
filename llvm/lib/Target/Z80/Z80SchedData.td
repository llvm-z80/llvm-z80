//===-- Z80SchedData.td - Z80 Instruction Timing Data ------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Comprehensive Z80 instruction timing data (T-states) for scheduling and
// cost models. All values are for the standard Z80 CPU (NMOS/CMOS).
//
// Sources:
//   - Zilog Z80 CPU User Manual (UM0080)
//   - https://map.grauw.nl/resources/z80instr.php
//   - https://clrhome.org/table/
//
// Notation:
//   - Bytes: instruction size in bytes
//   - TStates: clock cycles (T-states)
//   - For conditional instructions: Taken/NotTaken T-states
//   - "r" = register (B,C,D,E,H,L,A)
//   - "rr" = register pair (BC,DE,HL,SP)
//   - "n" = 8-bit immediate
//   - "nn" = 16-bit immediate
//   - "cc" = condition code (NZ,Z,NC,C,PO,PE,P,M)
//   - "(HL)" = indirect through HL
//   - "(IX+d)" = indexed addressing (same for IY)
//   - "b" = bit number 0-7
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Scheduling model definition
//===----------------------------------------------------------------------===//

def Z80Model : SchedMachineModel {
  let MicroOpBufferSize = 0;  // Z80 is strictly in-order, no buffering
  let IssueWidth = 1;         // One instruction at a time
  let LoadLatency = 3;        // Memory read takes ~3 T-states within instr
  let MispredictPenalty = 0;  // No branch prediction
  let CompleteModel = 0;      // Allow instructions without sched info
}

//===----------------------------------------------------------------------===//
// Scheduling resource: single execution unit
//===----------------------------------------------------------------------===//

let SchedModel = Z80Model in {

def Z80Unit : ProcResource<1>;

//===----------------------------------------------------------------------===//
// Scheduler write types — one per distinct T-state cost
//
// Naming: Z80Write<TStates>  (or Z80Write<Taken>_<NotTaken> for conditional)
//===----------------------------------------------------------------------===//

// --- 4 T-states ---
// LD r,r / ADD A,r / SUB r / AND r / OR r / XOR r / CP r
// INC r / DEC r / RLCA / RLA / RRCA / RRA
// EX DE,HL / EX AF,AF' / EXX / JP (HL)
// NOP / DAA / CPL / SCF / CCF / HALT / DI / EI
def Z80Write4 : SchedWriteRes<[Z80Unit]> {
  let Latency = 4;
  let ReleaseAtCycles = [4];
}

// --- 6 T-states ---
// INC rr / DEC rr / LD SP,HL
def Z80Write6 : SchedWriteRes<[Z80Unit]> {
  let Latency = 6;
  let ReleaseAtCycles = [6];
}

// --- 7 T-states ---
// LD r,n / LD r,(HL) / LD (HL),r / ADD A,n / SUB n / AND n / OR n
// XOR n / CP n / ADC A,n / SBC A,n / ADD A,(HL) / SUB (HL) etc.
// LD A,(BC) / LD A,(DE) / LD (BC),A / LD (DE),A
def Z80Write7 : SchedWriteRes<[Z80Unit]> {
  let Latency = 7;
  let ReleaseAtCycles = [7];
}

// --- 8 T-states ---
// NEG / RLC r / RL r / RRC r / RR r / SLA r / SRA r / SRL r
// BIT b,r / SET b,r / RES b,r / IM 0/1/2
// JP (IX) / JP (IY)
def Z80Write8 : SchedWriteRes<[Z80Unit]> {
  let Latency = 8;
  let ReleaseAtCycles = [8];
}

// --- 9 T-states ---
// LD A,I / LD A,R / LD I,A / LD R,A
def Z80Write9 : SchedWriteRes<[Z80Unit]> {
  let Latency = 9;
  let ReleaseAtCycles = [9];
}

// --- 10 T-states ---
// JP nn / JP cc,nn / LD rr,nn / POP rr
// LD SP,IX / LD SP,IY / INC IX / INC IY / DEC IX / DEC IY
// LD (HL),n
def Z80Write10 : SchedWriteRes<[Z80Unit]> {
  let Latency = 10;
  let ReleaseAtCycles = [10];
}

// --- 11 T-states ---
// PUSH rr / RST p / ADD HL,rr / INC (HL) / DEC (HL)
// IN A,(n) / OUT (n),A / RET (unconditional)
def Z80Write11 : SchedWriteRes<[Z80Unit]> {
  let Latency = 11;
  let ReleaseAtCycles = [11];
}

// --- 12 T-states ---
// JR e (unconditional) / BIT b,(HL) / IN r,(C) / OUT (C),r
def Z80Write12 : SchedWriteRes<[Z80Unit]> {
  let Latency = 12;
  let ReleaseAtCycles = [12];
}

// --- 13 T-states ---
// LD A,(nn) / LD (nn),A / DJNZ (taken)
def Z80Write13 : SchedWriteRes<[Z80Unit]> {
  let Latency = 13;
  let ReleaseAtCycles = [13];
}

// --- 14 T-states ---
// LD IX,nn / LD IY,nn / POP IX / POP IY / RETI / RETN
def Z80Write14 : SchedWriteRes<[Z80Unit]> {
  let Latency = 14;
  let ReleaseAtCycles = [14];
}

// --- 15 T-states ---
// ADC HL,rr / SBC HL,rr / ADD IX,rr / ADD IY,rr
// PUSH IX / PUSH IY
// RLC (HL) / RL (HL) / RRC (HL) / RR (HL) / SLA (HL) / SRA (HL) / SRL (HL)
// SET b,(HL) / RES b,(HL)
def Z80Write15 : SchedWriteRes<[Z80Unit]> {
  let Latency = 15;
  let ReleaseAtCycles = [15];
}

// --- 16 T-states ---
// LD HL,(nn) / LD (nn),HL / LDI / LDD / CPI / CPD / INI / IND / OUTI / OUTD
def Z80Write16 : SchedWriteRes<[Z80Unit]> {
  let Latency = 16;
  let ReleaseAtCycles = [16];
}

// --- 17 T-states ---
// CALL nn / CALL cc,nn (taken)
def Z80Write17 : SchedWriteRes<[Z80Unit]> {
  let Latency = 17;
  let ReleaseAtCycles = [17];
}

// --- 18 T-states ---
// RLD / RRD
def Z80Write18 : SchedWriteRes<[Z80Unit]> {
  let Latency = 18;
  let ReleaseAtCycles = [18];
}

// --- 19 T-states ---
// LD r,(IX+d) / LD (IX+d),r / LD (IX+d),n
// ADD A,(IX+d) / SUB (IX+d) / AND (IX+d) / OR (IX+d) etc.
// EX (SP),HL
def Z80Write19 : SchedWriteRes<[Z80Unit]> {
  let Latency = 19;
  let ReleaseAtCycles = [19];
}

// --- 20 T-states ---
// LD rr,(nn) [ED-prefixed] / LD (nn),rr [ED-prefixed]
// LD IX,(nn) / LD (nn),IX / LD IY,(nn) / LD (nn),IY
// BIT b,(IX+d) / BIT b,(IY+d)
def Z80Write20 : SchedWriteRes<[Z80Unit]> {
  let Latency = 20;
  let ReleaseAtCycles = [20];
}

// --- 21 T-states ---
// LDIR/LDDR/CPIR/CPDR/INIR/INDR/OTIR/OTDR (per iteration, repeat)
def Z80Write21 : SchedWriteRes<[Z80Unit]> {
  let Latency = 21;
  let ReleaseAtCycles = [21];
}

// --- 23 T-states ---
// INC (IX+d) / DEC (IX+d) / EX (SP),IX / EX (SP),IY
// RLC (IX+d) / RL (IX+d) / RRC (IX+d) / RR (IX+d)
// SLA (IX+d) / SRA (IX+d) / SRL (IX+d)
// SET b,(IX+d) / RES b,(IX+d)
def Z80Write23 : SchedWriteRes<[Z80Unit]> {
  let Latency = 23;
  let ReleaseAtCycles = [23];
}

//===----------------------------------------------------------------------===//
// Conditional branch write types: Taken / NotTaken
//===----------------------------------------------------------------------===//

// JR cc,e: 12 taken / 7 not-taken
def Z80WriteJRcc : SchedWriteRes<[Z80Unit]> {
  let Latency = 12;  // Worst case (taken)
  let ReleaseAtCycles = [12];
}

// DJNZ e: 13 taken / 8 not-taken
def Z80WriteDJNZ : SchedWriteRes<[Z80Unit]> {
  let Latency = 13;  // Worst case (taken)
  let ReleaseAtCycles = [13];
}

// JP cc,nn: 10 always (same whether taken or not)
// (Uses Z80Write10 above)

// CALL cc,nn: 17 taken / 10 not-taken
def Z80WriteCALLcc : SchedWriteRes<[Z80Unit]> {
  let Latency = 17;  // Worst case (taken)
  let ReleaseAtCycles = [17];
}

// RET cc: 11 taken / 5 not-taken
def Z80WriteRETcc : SchedWriteRes<[Z80Unit]> {
  let Latency = 11;  // Worst case (taken)
  let ReleaseAtCycles = [11];
}

} // let SchedModel = Z80Model

//===----------------------------------------------------------------------===//
// Complete Z80 Instruction Timing Reference
//===----------------------------------------------------------------------===//
//
// This table documents every Z80 instruction with size and T-state count.
// It serves as the authoritative reference for the scheduling model above.
//
// For conditional instructions, format is: Taken/NotTaken
//
// =====================================================================
// 8-BIT LOAD GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// LD r,r'                1      4      Z80Write4
// LD r,n                 2      7      Z80Write7
// LD r,(HL)              1      7      Z80Write7
// LD r,(IX+d)            3     19      Z80Write19
// LD r,(IY+d)            3     19      Z80Write19
// LD (HL),r              1      7      Z80Write7
// LD (HL),n              2     10      Z80Write10
// LD (IX+d),r            3     19      Z80Write19
// LD (IX+d),n            4     19      Z80Write19
// LD (IY+d),r            3     19      Z80Write19
// LD (IY+d),n            4     19      Z80Write19
// LD A,(BC)              1      7      Z80Write7
// LD A,(DE)              1      7      Z80Write7
// LD A,(nn)              3     13      Z80Write13
// LD (BC),A              1      7      Z80Write7
// LD (DE),A              1      7      Z80Write7
// LD (nn),A              3     13      Z80Write13
// LD A,I                 2      9      Z80Write9
// LD A,R                 2      9      Z80Write9
// LD I,A                 2      9      Z80Write9
// LD R,A                 2      9      Z80Write9
//
// =====================================================================
// 16-BIT LOAD GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// LD BC,nn               3     10      Z80Write10
// LD DE,nn               3     10      Z80Write10
// LD HL,nn               3     10      Z80Write10
// LD SP,nn               3     10      Z80Write10
// LD IX,nn               4     14      Z80Write14
// LD IY,nn               4     14      Z80Write14
// LD HL,(nn)             3     16      Z80Write16
// LD BC,(nn)             4     20      Z80Write20
// LD DE,(nn)             4     20      Z80Write20
// LD SP,(nn)             4     20      Z80Write20
// LD IX,(nn)             4     20      Z80Write20
// LD IY,(nn)             4     20      Z80Write20
// LD (nn),HL             3     16      Z80Write16
// LD (nn),BC             4     20      Z80Write20
// LD (nn),DE             4     20      Z80Write20
// LD (nn),SP             4     20      Z80Write20
// LD (nn),IX             4     20      Z80Write20
// LD (nn),IY             4     20      Z80Write20
// LD SP,HL               1      6      Z80Write6
// LD SP,IX               2     10      Z80Write10
// LD SP,IY               2     10      Z80Write10
// PUSH AF                1     11      Z80Write11
// PUSH BC                1     11      Z80Write11
// PUSH DE                1     11      Z80Write11
// PUSH HL                1     11      Z80Write11
// PUSH IX                2     15      Z80Write15
// PUSH IY                2     15      Z80Write15
// POP AF                 1     10      Z80Write10
// POP BC                 1     10      Z80Write10
// POP DE                 1     10      Z80Write10
// POP HL                 1     10      Z80Write10
// POP IX                 2     14      Z80Write14
// POP IY                 2     14      Z80Write14
//
// =====================================================================
// 8-BIT ARITHMETIC / LOGIC GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// ADD A,r                1      4      Z80Write4
// ADD A,n                2      7      Z80Write7
// ADD A,(HL)             1      7      Z80Write7
// ADD A,(IX+d)           3     19      Z80Write19
// ADD A,(IY+d)           3     19      Z80Write19
// ADC A,r                1      4      Z80Write4
// ADC A,n                2      7      Z80Write7
// ADC A,(HL)             1      7      Z80Write7
// ADC A,(IX+d)           3     19      Z80Write19
// ADC A,(IY+d)           3     19      Z80Write19
// SUB r                  1      4      Z80Write4
// SUB n                  2      7      Z80Write7
// SUB (HL)               1      7      Z80Write7
// SUB (IX+d)             3     19      Z80Write19
// SUB (IY+d)             3     19      Z80Write19
// SBC A,r                1      4      Z80Write4
// SBC A,n                2      7      Z80Write7
// SBC A,(HL)             1      7      Z80Write7
// SBC A,(IX+d)           3     19      Z80Write19
// SBC A,(IY+d)           3     19      Z80Write19
// AND r                  1      4      Z80Write4
// AND n                  2      7      Z80Write7
// AND (HL)               1      7      Z80Write7
// AND (IX+d)             3     19      Z80Write19
// AND (IY+d)             3     19      Z80Write19
// OR r                   1      4      Z80Write4
// OR n                   2      7      Z80Write7
// OR (HL)                1      7      Z80Write7
// OR (IX+d)              3     19      Z80Write19
// OR (IY+d)              3     19      Z80Write19
// XOR r                  1      4      Z80Write4
// XOR n                  2      7      Z80Write7
// XOR (HL)               1      7      Z80Write7
// XOR (IX+d)             3     19      Z80Write19
// XOR (IY+d)             3     19      Z80Write19
// CP r                   1      4      Z80Write4
// CP n                   2      7      Z80Write7
// CP (HL)                1      7      Z80Write7
// CP (IX+d)              3     19      Z80Write19
// CP (IY+d)              3     19      Z80Write19
// INC r                  1      4      Z80Write4
// INC (HL)               1     11      Z80Write11
// INC (IX+d)             3     23      Z80Write23
// INC (IY+d)             3     23      Z80Write23
// DEC r                  1      4      Z80Write4
// DEC (HL)               1     11      Z80Write11
// DEC (IX+d)             3     23      Z80Write23
// DEC (IY+d)             3     23      Z80Write23
// DAA                    1      4      Z80Write4
// CPL                    1      4      Z80Write4
// NEG                    2      8      Z80Write8
// SCF                    1      4      Z80Write4
// CCF                    1      4      Z80Write4
//
// =====================================================================
// 16-BIT ARITHMETIC GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// ADD HL,BC              1     11      Z80Write11
// ADD HL,DE              1     11      Z80Write11
// ADD HL,HL              1     11      Z80Write11
// ADD HL,SP              1     11      Z80Write11
// ADC HL,BC              2     15      Z80Write15
// ADC HL,DE              2     15      Z80Write15
// ADC HL,HL              2     15      Z80Write15
// ADC HL,SP              2     15      Z80Write15
// SBC HL,BC              2     15      Z80Write15
// SBC HL,DE              2     15      Z80Write15
// SBC HL,HL              2     15      Z80Write15
// SBC HL,SP              2     15      Z80Write15
// ADD IX,BC              2     15      Z80Write15
// ADD IX,DE              2     15      Z80Write15
// ADD IX,IX              2     15      Z80Write15
// ADD IX,SP              2     15      Z80Write15
// ADD IY,BC              2     15      Z80Write15
// ADD IY,DE              2     15      Z80Write15
// ADD IY,IY              2     15      Z80Write15
// ADD IY,SP              2     15      Z80Write15
// INC BC                 1      6      Z80Write6
// INC DE                 1      6      Z80Write6
// INC HL                 1      6      Z80Write6
// INC SP                 1      6      Z80Write6
// INC IX                 2     10      Z80Write10
// INC IY                 2     10      Z80Write10
// DEC BC                 1      6      Z80Write6
// DEC DE                 1      6      Z80Write6
// DEC HL                 1      6      Z80Write6
// DEC SP                 1      6      Z80Write6
// DEC IX                 2     10      Z80Write10
// DEC IY                 2     10      Z80Write10
//
// =====================================================================
// ROTATE AND SHIFT GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// RLCA                   1      4      Z80Write4
// RLA                    1      4      Z80Write4
// RRCA                   1      4      Z80Write4
// RRA                    1      4      Z80Write4
// RLC r                  2      8      Z80Write8
// RLC (HL)               2     15      Z80Write15
// RLC (IX+d)             4     23      Z80Write23
// RLC (IY+d)             4     23      Z80Write23
// RL r                   2      8      Z80Write8
// RL (HL)                2     15      Z80Write15
// RL (IX+d)              4     23      Z80Write23
// RL (IY+d)              4     23      Z80Write23
// RRC r                  2      8      Z80Write8
// RRC (HL)               2     15      Z80Write15
// RRC (IX+d)             4     23      Z80Write23
// RRC (IY+d)             4     23      Z80Write23
// RR r                   2      8      Z80Write8
// RR (HL)                2     15      Z80Write15
// RR (IX+d)              4     23      Z80Write23
// RR (IY+d)              4     23      Z80Write23
// SLA r                  2      8      Z80Write8
// SLA (HL)               2     15      Z80Write15
// SLA (IX+d)             4     23      Z80Write23
// SLA (IY+d)             4     23      Z80Write23
// SRA r                  2      8      Z80Write8
// SRA (HL)               2     15      Z80Write15
// SRA (IX+d)             4     23      Z80Write23
// SRA (IY+d)             4     23      Z80Write23
// SRL r                  2      8      Z80Write8
// SRL (HL)               2     15      Z80Write15
// SRL (IX+d)             4     23      Z80Write23
// SRL (IY+d)             4     23      Z80Write23
// RLD                    2     18      Z80Write18
// RRD                    2     18      Z80Write18
//
// =====================================================================
// BIT MANIPULATION GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// BIT b,r                2      8      Z80Write8
// BIT b,(HL)             2     12      Z80Write12
// BIT b,(IX+d)           4     20      Z80Write20
// BIT b,(IY+d)           4     20      Z80Write20
// SET b,r                2      8      Z80Write8
// SET b,(HL)             2     15      Z80Write15
// SET b,(IX+d)           4     23      Z80Write23
// SET b,(IY+d)           4     23      Z80Write23
// RES b,r                2      8      Z80Write8
// RES b,(HL)             2     15      Z80Write15
// RES b,(IX+d)           4     23      Z80Write23
// RES b,(IY+d)           4     23      Z80Write23
//
// =====================================================================
// JUMP GROUP
// =====================================================================
// Instruction          Bytes  TStates       Sched Class
// ---------------------------------------------------------------------
// JP nn                  3     10           Z80Write10
// JP cc,nn               3     10           Z80Write10
//   (same T-states whether condition met or not)
// JP (HL)                1      4           Z80Write4
// JP (IX)                2      8           Z80Write8
// JP (IY)                2      8           Z80Write8
// JR e                   2     12           Z80Write12
// JR cc,e                2     12/7         Z80WriteJRcc
// DJNZ e                 2     13/8         Z80WriteDJNZ
//
// =====================================================================
// CALL AND RETURN GROUP
// =====================================================================
// Instruction          Bytes  TStates       Sched Class
// ---------------------------------------------------------------------
// CALL nn                3     17           Z80Write17
// CALL cc,nn             3     17/10        Z80WriteCALLcc
// RET                    1     10           Z80Write10
// RET cc                 1     11/5         Z80WriteRETcc
// RETI                   2     14           Z80Write14
// RETN                   2     14           Z80Write14
// RST p                  1     11           Z80Write11
//
// =====================================================================
// EXCHANGE GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// EX DE,HL               1      4      Z80Write4
// EX AF,AF'              1      4      Z80Write4
// EXX                    1      4      Z80Write4
// EX (SP),HL             1     19      Z80Write19
// EX (SP),IX             2     23      Z80Write23
// EX (SP),IY             2     23      Z80Write23
//
// =====================================================================
// BLOCK TRANSFER AND SEARCH GROUP
// =====================================================================
// Instruction          Bytes  TStates       Sched Class
// ---------------------------------------------------------------------
// LDI                    2     16           Z80Write16
// LDIR                   2     21/16        Z80Write21
//   (21 per iteration while BC!=0, 16 on final iteration)
// LDD                    2     16           Z80Write16
// LDDR                   2     21/16        Z80Write21
// CPI                    2     16           Z80Write16
// CPIR                   2     21/16        Z80Write21
// CPD                    2     16           Z80Write16
// CPDR                   2     21/16        Z80Write21
//
// =====================================================================
// INPUT / OUTPUT GROUP
// =====================================================================
// Instruction          Bytes  TStates       Sched Class
// ---------------------------------------------------------------------
// IN A,(n)               2     11           Z80Write11
// IN r,(C)               2     12           Z80Write12
// INI                    2     16           Z80Write16
// INIR                   2     21/16        Z80Write21
// IND                    2     16           Z80Write16
// INDR                   2     21/16        Z80Write21
// OUT (n),A              2     11           Z80Write11
// OUT (C),r              2     12           Z80Write12
// OUTI                   2     16           Z80Write16
// OTIR                   2     21/16        Z80Write21
// OUTD                   2     16           Z80Write16
// OTDR                   2     21/16        Z80Write21
//
// =====================================================================
// CONTROL GROUP
// =====================================================================
// Instruction          Bytes  TStates  Sched Class
// ---------------------------------------------------------------------
// NOP                    1      4      Z80Write4
// HALT                   1      4      Z80Write4
// DI                     1      4      Z80Write4
// EI                     1      4      Z80Write4
// IM 0                   2      8      Z80Write8
// IM 1                   2      8      Z80Write8
// IM 2                   2      8      Z80Write8
//
// =====================================================================
// COST MODEL SUMMARY — useful for LLVM backend optimization decisions
// =====================================================================
//
// Key patterns for the cost model:
//
// Register-register ops are cheapest:
//   LD r,r / ADD A,r / etc.         = 4 T / 1 byte  (cost 1.0)
//
// Immediate ops are moderate:
//   LD r,n / ADD A,n / etc.         = 7 T / 2 bytes  (cost 1.75)
//
// (HL) indirect ops are moderate:
//   LD r,(HL) / ADD A,(HL) / etc.   = 7 T / 1 byte   (cost 1.75)
//   INC (HL) / DEC (HL)             = 11 T / 1 byte  (cost 2.75)
//
// IX/IY indexed ops are expensive:
//   LD r,(IX+d) / ADD A,(IX+d)      = 19 T / 3 bytes (cost 4.75)
//   INC (IX+d) / DEC (IX+d)         = 23 T / 3 bytes (cost 5.75)
//   Shift/rotate (IX+d)             = 23 T / 4 bytes (cost 5.75)
//
// 16-bit ops:
//   INC rr / DEC rr                 = 6 T / 1 byte   (cost 1.5)
//   ADD HL,rr                       = 11 T / 1 byte  (cost 2.75)
//   ADC/SBC HL,rr                   = 15 T / 2 bytes (cost 3.75)
//
// Branches:
//   JP nn                           = 10 T / 3 bytes (cost 2.5)
//   JR e                            = 12 T / 2 bytes (cost 3.0)
//   JR cc,e  (not taken)            = 7 T  / 2 bytes (cost 1.75)
//   DJNZ     (taken)                = 13 T / 2 bytes (cost 3.25)
//   CALL nn                         = 17 T / 3 bytes (cost 4.25)
//   RET                             = 10 T / 1 byte  (cost 2.5)
//
// Stack:
//   PUSH rr                         = 11 T / 1 byte  (cost 2.75)
//   POP rr                          = 10 T / 1 byte  (cost 2.5)
//   PUSH IX/IY                      = 15 T / 2 bytes (cost 3.75)
//   POP IX/IY                       = 14 T / 2 bytes (cost 3.5)
//
// Exchange (free register swaps!):
//   EX DE,HL / EXX / EX AF,AF'      = 4 T / 1 byte  (cost 1.0)
//   EX (SP),HL                      = 19 T / 1 byte  (cost 4.75)
//
// Block operations (per byte transferred):
//   LDI/LDD (single)                = 16 T / 2 bytes (cost 4.0)
//   LDIR/LDDR (per iteration)       = 21 T / 2 bytes (cost 5.25)
//   LDIR final iteration            = 16 T           (cost 4.0)
//
// "Cost" above = TStates/4, normalized to register-register = 1.0
//
