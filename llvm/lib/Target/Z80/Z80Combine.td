//===-- Z80Combine.td - Describe Z80 combiner -------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

// srem a, b -> sext (G_Z80_SREM8 a, b)
def narrow_srem : GICombineRule<
  (defs root:$dst),
  (match (G_SREM i16:$dst, $a, $b):$rem,
         [{ return canDivideInByte(*${rem}, Helper); }]),
  (apply (G_TRUNC i8:$na, $a), (G_TRUNC i8:$nb, $b),
         (G_Z80_SREM8 i8:$r, $na, $nb),
         (G_SEXT $dst, $r))>;

// trunc (sdiv a, b) -> G_Z80_SDIV8 a, b
def narrow_sdiv_trunc : GICombineRule<
  (defs root:$dst),
  (match (G_SDIV i16:$q, $a, $b):$div,
         (GIHasOneUse $q),
         (G_TRUNC i8:$dst, $q),
         [{ return canDivideInByte(*${div}, Helper); }]),
  (apply (G_TRUNC i8:$na, $a), (G_TRUNC i8:$nb, $b),
         (G_Z80_SDIV8 $dst, $na, $nb))>;

// and (sdiv a, b), 0xFF -> zext (G_Z80_SDIV8 a, b)
def narrow_sdiv_mask : GICombineRule<
  (defs root:$dst),
  (match (G_SDIV i16:$q, $a, $b):$div,
         (GIHasOneUse $q),
         (G_AND $dst, $q, 255),
         [{ return canDivideInByte(*${div}, Helper); }]),
  (apply (G_TRUNC i8:$na, $a), (G_TRUNC i8:$nb, $b),
         (G_Z80_SDIV8 i8:$r, $na, $nb),
         (G_ZEXT $dst, $r))>;

def narrow_byte_division : GICombineGroup<
  [narrow_srem, narrow_sdiv_trunc, narrow_sdiv_mask]>;

// Pre-legalization: the byte division rules and all standard combines.
def Z80PreLegalizerCombiner : GICombiner<
  "Z80PreLegalizerCombinerImpl", [narrow_byte_division, all_combines]>;

// ptr_add (global_value @g + o), c -> global_value @g + (o + c)
def fold_global_offset_matchdata :
    GIDefMatchData<"std::pair<const GlobalValue *, int64_t>">;
def fold_global_offset : GICombineRule<
  (defs root:$root, fold_global_offset_matchdata:$matchinfo),
  (match (wip_match_opcode G_PTR_ADD):$root,
         [{ return matchFoldGlobalOffset(*${root}, MRI, ${matchinfo}); }]),
  (apply [{ applyFoldGlobalOffset(*${root}, B, ${matchinfo}); }])>;

// Post-legalization: selective combines that preserve legality.
def Z80PostLegalizerCombiner : GICombiner<
  "Z80PostLegalizerCombinerImpl",
  [copy_prop, cast_of_cast_combines, identity_combines, merge_combines,
   commute_constant_to_rhs, redundant_and, fold_global_offset]>;
