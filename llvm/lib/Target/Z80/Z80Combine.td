//===-- Z80Combine.td - Describe Z80 combiner -------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

// srem (sext a), (sext b) -> sext (G_Z80_SREM8 a, b)
// low byte of sdiv (sext a), (sext b) -> G_Z80_SDIV8 a, b
def narrow_sdiv_srem_of_sext : GICombineRule<
  (defs root:$root, build_fn_matchinfo:$matchinfo),
  (match (wip_match_opcode G_SREM, G_TRUNC, G_AND):$root,
         [{ return matchNarrowSDivSRemOfSExt(*${root}, MRI, ${matchinfo}); }]),
  (apply [{ Helper.applyBuildFn(*${root}, ${matchinfo}); }])>;

// Pre-legalization: all standard combines.
def Z80PreLegalizerCombiner : GICombiner<
  "Z80PreLegalizerCombinerImpl", [narrow_sdiv_srem_of_sext, all_combines]>;

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
