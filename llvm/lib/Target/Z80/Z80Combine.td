//===-- Z80Combine.td - Describe Z80 combiner -------------*- tablegen -*-===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

// Pre-legalization: use all standard combines.
def Z80PreLegalizerCombiner : GICombiner<
  "Z80PreLegalizerCombinerImpl", [all_combines]>;

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
