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

// Custom rule: canonicalize cross-size COPYs to G_TRUNC/G_ANYEXT.
// The legalizer's narrowScalar can produce COPYs between different-sized
// virtual registers (e.g. s16 -> s8). ISel requires G_TRUNC/G_ANYEXT instead.
def z80_cross_size_copy_matchdata : GIDefMatchData<"unsigned">;
def z80_cross_size_copy : GICombineRule<
  (defs root:$dst, z80_cross_size_copy_matchdata:$matchinfo),
  (match (COPY $dst, $src):$mi,
         [{ return matchCrossSizeCopy(*${mi}, MRI, ${matchinfo}); }]),
  (apply [{ applyCrossSizeCopy(*${mi}, ${matchinfo}); }])>;

// Post-legalization: selective combines that preserve legality, plus
// Z80-specific cross-size COPY canonicalization.
def Z80PostLegalizerCombiner : GICombiner<
  "Z80PostLegalizerCombinerImpl",
  [copy_prop, cast_of_cast_combines, identity_combines, merge_combines,
   commute_constant_to_rhs, z80_cross_size_copy]>;
