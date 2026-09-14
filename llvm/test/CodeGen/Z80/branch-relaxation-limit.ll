; RUN: llc -verify-machineinstrs -mtriple=z80 -O3 -filetype=obj < %s -o /dev/null
;
; Reduced from Rust core::fmt. Branch relaxation leaves this function with
; branches sitting on both limits at once, forward at 127 and back at 128,
; which is where a one-off in the assembler's relaxation decision showed:
; the forward one was widened though it fitted, and the byte that added
; pushed the backward one out of reach of a check that no longer caught it.
; The point of the test is that it assembles at all, so it has no CHECK.


@g = external constant [1 x i8]

define fastcc void @branch_at_limit(ptr %_0, i1 %sign, i16 %frac_digits, ptr %buf.0, ptr %parts.0, ptr %0, i8 %narrow, i1 %negative) #0 {
start:
  %1 = icmp eq i8 %narrow, 0
  br i1 %1, label %bb9, label %bb36

bb36:                                             ; preds = %start
  %2 = load i8, ptr %0, align 1
  %3 = load i8, ptr %_0, align 1
  %p1 = select i1 %negative, ptr @g, ptr null
  %negative4 = trunc i8 %3 to i1
  %p2 = select i1 %negative4, ptr @g, ptr null
  %. = zext i8 %3 to i16
  %sign1.sroa.12.1 = select i1 %sign, i16 0, i16 %.
  %sign1.sroa.0.15 = select i1 %sign, ptr %p1, ptr %p2
  switch i8 %2, label %bb5 [
    i8 2, label %bb6
    i8 1, label %bb10
  ]

bb6:                                              ; preds = %bb36
  store i8 1, ptr %parts.0, align 1
  %_33.sroa.4.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 1
  %_33.sroa.5.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 3
  %_24.not = icmp eq i16 %frac_digits, 0
  br i1 %_24.not, label %bb15, label %bb12

bb5:                                              ; preds = %bb36
  call void @callee(ptr %buf.0, ptr %_0)
  br i1 %sign, label %bb33, label %bb23

bb9:                                              ; preds = %start
  store i8 1, ptr %_0, align 1
  store ptr null, ptr %buf.0, align 1
  %_14.sroa.5.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 3
  store i16 0, ptr %_14.sroa.5.0..sroa_idx, align 1
  br label %bb33

bb33:                                             ; preds = %bb25, %bb28, %bb12, %bb15, %bb10, %bb9, %bb5
  %sign1.sroa.0.1.sink = phi ptr [ %sign1.sroa.0.15, %bb28 ], [ %sign1.sroa.0.15, %bb25 ], [ null, %bb9 ], [ %sign1.sroa.0.15, %bb15 ], [ %sign1.sroa.0.15, %bb12 ], [ %sign1.sroa.0.15, %bb10 ], [ null, %bb5 ]
  %sign1.sroa.12.1.sink = phi i16 [ %sign1.sroa.12.1, %bb28 ], [ %sign1.sroa.12.1, %bb25 ], [ 0, %bb9 ], [ %sign1.sroa.12.1, %bb15 ], [ %sign1.sroa.12.1, %bb12 ], [ %sign1.sroa.12.1, %bb10 ], [ 0, %bb5 ]
  %parts.0.sink = phi ptr [ %parts.0, %bb28 ], [ %parts.0, %bb25 ], [ %parts.0, %bb9 ], [ %parts.0, %bb15 ], [ %parts.0, %bb12 ], [ %parts.0, %bb10 ], [ null, %bb5 ]
  store ptr %sign1.sroa.0.1.sink, ptr %_0, align 1
  store i16 %sign1.sroa.12.1.sink, ptr %_0, align 1
  store ptr %parts.0.sink, ptr %_0, align 1
  ret void

bb10:                                             ; preds = %bb36
  store i8 1, ptr %parts.0, align 1
  %_19.sroa.4.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 1
  store ptr null, ptr %_19.sroa.4.0..sroa_idx, align 1
  %_19.sroa.5.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 3
  store i16 0, ptr %_19.sroa.5.0..sroa_idx, align 1
  br label %bb33

bb15:                                             ; preds = %bb6
  store ptr null, ptr %_33.sroa.4.0..sroa_idx, align 1
  store i16 1, ptr %_33.sroa.5.0..sroa_idx, align 1
  br label %bb33

bb12:                                             ; preds = %bb6
  store ptr null, ptr %_33.sroa.4.0..sroa_idx, align 1
  store i16 2, ptr %_33.sroa.5.0..sroa_idx, align 1
  %4 = getelementptr i8, ptr %parts.0, i16 5
  store i8 0, ptr %4, align 1
  %_29.sroa.4.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 6
  store i16 %frac_digits, ptr %_29.sroa.4.0..sroa_idx, align 1
  br label %bb33

bb23:                                             ; preds = %bb5
  store i8 0, ptr %parts.0, align 1
  %_70.sroa.4.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 1
  %_70.sroa.5.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 3
  %_61.not = icmp eq i16 %frac_digits, 0
  br i1 %_61.not, label %bb28, label %bb25

bb28:                                             ; preds = %bb23
  store ptr null, ptr %_70.sroa.4.0..sroa_idx, align 1
  store i16 1, ptr %_70.sroa.5.0..sroa_idx, align 1
  br label %bb33

bb25:                                             ; preds = %bb23
  store ptr null, ptr %_70.sroa.4.0..sroa_idx, align 1
  store i16 2, ptr %_70.sroa.5.0..sroa_idx, align 1
  %5 = getelementptr i8, ptr %parts.0, i16 5
  store i8 0, ptr %5, align 1
  %_66.sroa.4.0..sroa_idx = getelementptr i8, ptr %parts.0, i16 6
  store i16 %frac_digits, ptr %_66.sroa.4.0..sroa_idx, align 1
  br label %bb33
}

declare void @callee(ptr, ptr)

attributes #0 = { "frame-pointer"="non-leaf" }
