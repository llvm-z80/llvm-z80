; Test 24: aggregate (struct) register return
; Tests small struct returns packed directly into registers.
; Covers {i1, i16}, {i8, i8}, {i16, i16}, {i16, i8}, {i8, i16},
; {i8, i8, i16}, {i8, i8, i8} return types.
; expect 0x00FF (8 bits, all pass)

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

; --- Callee functions that return structs ---

; {i1, i16}: Rust's Option<u16>-like type (the original crash case)
define {i1, i16} @make_option(i16 %val, i1 %has_val) {
  %r0 = insertvalue {i1, i16} undef, i1 %has_val, 0
  %r1 = insertvalue {i1, i16} %r0, i16 %val, 1
  ret {i1, i16} %r1
}

; {i8, i8}: simple 2-byte struct
define {i8, i8} @make_pair_i8(i8 %a, i8 %b) {
  %r0 = insertvalue {i8, i8} undef, i8 %a, 0
  %r1 = insertvalue {i8, i8} %r0, i8 %b, 1
  ret {i8, i8} %r1
}

; {i16, i16}: 4-byte struct, uses i32 return registers
define {i16, i16} @make_pair_i16(i16 %a, i16 %b) {
  %r0 = insertvalue {i16, i16} undef, i16 %a, 0
  %r1 = insertvalue {i16, i16} %r0, i16 %b, 1
  ret {i16, i16} %r1
}

; {i16, i8}: 3-byte struct, field 0 in Lo, field 1 anyext in Hi
define {i16, i8} @make_mixed(i16 %a, i8 %b) {
  %r0 = insertvalue {i16, i8} undef, i16 %a, 0
  %r1 = insertvalue {i16, i8} %r0, i8 %b, 1
  ret {i16, i8} %r1
}

; {i8, i16}: 3-byte struct, field 0 anyext in Lo, field 1 in Hi
define {i8, i16} @make_mixed2(i8 %a, i16 %b) {
  %r0 = insertvalue {i8, i16} undef, i8 %a, 0
  %r1 = insertvalue {i8, i16} %r0, i16 %b, 1
  ret {i8, i16} %r1
}

; {i8, i8, i16}: 4-byte struct, merge(f0,f1) in Lo, f2 in Hi
define {i8, i8, i16} @make_triple(i8 %a, i8 %b, i16 %c) {
  %r0 = insertvalue {i8, i8, i16} undef, i8 %a, 0
  %r1 = insertvalue {i8, i8, i16} %r0, i8 %b, 1
  %r2 = insertvalue {i8, i8, i16} %r1, i16 %c, 2
  ret {i8, i8, i16} %r2
}

; {i8, i8, i8}: 3-byte struct, merge(f0,f1) in Lo, f2 anyext in Hi
define {i8, i8, i8} @make_triple_i8(i8 %a, i8 %b, i8 %c) {
  %r0 = insertvalue {i8, i8, i8} undef, i8 %a, 0
  %r1 = insertvalue {i8, i8, i8} %r0, i8 %b, 1
  %r2 = insertvalue {i8, i8, i8} %r1, i8 %c, 2
  ret {i8, i8, i8} %r2
}

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: {i1, i16} return — check both fields
  %opt = call {i1, i16} @make_option(i16 12345, i1 true)
  %opt_flag = extractvalue {i1, i16} %opt, 0
  %opt_val = extractvalue {i1, i16} %opt, 1
  %opt_flag_ok = icmp eq i1 %opt_flag, true
  %opt_val_ok = icmp eq i16 %opt_val, 12345
  %opt_ok = and i1 %opt_flag_ok, %opt_val_ok
  br i1 %opt_ok, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: {i8, i8} return
  %pair8 = call {i8, i8} @make_pair_i8(i8 42, i8 99)
  %p8_a = extractvalue {i8, i8} %pair8, 0
  %p8_b = extractvalue {i8, i8} %pair8, 1
  %p8a_ok = icmp eq i8 %p8_a, 42
  %p8b_ok = icmp eq i8 %p8_b, 99
  %p8_ok = and i1 %p8a_ok, %p8b_ok
  br i1 %p8_ok, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: {i16, i16} return
  %pair16 = call {i16, i16} @make_pair_i16(i16 1000, i16 2000)
  %p16_a = extractvalue {i16, i16} %pair16, 0
  %p16_b = extractvalue {i16, i16} %pair16, 1
  %p16a_ok = icmp eq i16 %p16_a, 1000
  %p16b_ok = icmp eq i16 %p16_b, 2000
  %p16_ok = and i1 %p16a_ok, %p16b_ok
  br i1 %p16_ok, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: {i16, i8} return
  %mixed = call {i16, i8} @make_mixed(i16 500, i8 77)
  %mx_a = extractvalue {i16, i8} %mixed, 0
  %mx_b = extractvalue {i16, i8} %mixed, 1
  %mxa_ok = icmp eq i16 %mx_a, 500
  %mxb_ok = icmp eq i8 %mx_b, 77
  %mx_ok = and i1 %mxa_ok, %mxb_ok
  br i1 %mx_ok, label %set3, label %t4
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %t4
t4:

  ; Bit 4: {i1, i16} with false flag — verify i1 false is preserved
  %opt_none = call {i1, i16} @make_option(i16 0, i1 false)
  %none_flag = extractvalue {i1, i16} %opt_none, 0
  %none_ok = icmp eq i1 %none_flag, false
  br i1 %none_ok, label %set4, label %t5
set4:
  %s4 = load i16, ptr %status
  %s4a = or i16 %s4, 16
  store i16 %s4a, ptr %status
  br label %t5
t5:

  ; Bit 5: {i8, i16} return — anyext field 0 in Lo, field 1 in Hi
  %mixed2 = call {i8, i16} @make_mixed2(i8 33, i16 7777)
  %m2_a = extractvalue {i8, i16} %mixed2, 0
  %m2_b = extractvalue {i8, i16} %mixed2, 1
  %m2a_ok = icmp eq i8 %m2_a, 33
  %m2b_ok = icmp eq i16 %m2_b, 7777
  %m2_ok = and i1 %m2a_ok, %m2b_ok
  br i1 %m2_ok, label %set5, label %t6
set5:
  %s5 = load i16, ptr %status
  %s5a = or i16 %s5, 32
  store i16 %s5a, ptr %status
  br label %t6
t6:

  ; Bit 6: {i8, i8, i16} return — merge(f0,f1) in Lo, f2 in Hi
  %triple = call {i8, i8, i16} @make_triple(i8 10, i8 20, i16 3000)
  %tr_a = extractvalue {i8, i8, i16} %triple, 0
  %tr_b = extractvalue {i8, i8, i16} %triple, 1
  %tr_c = extractvalue {i8, i8, i16} %triple, 2
  %tra_ok = icmp eq i8 %tr_a, 10
  %trb_ok = icmp eq i8 %tr_b, 20
  %trc_ok = icmp eq i16 %tr_c, 3000
  %trab_ok = and i1 %tra_ok, %trb_ok
  %tr_ok = and i1 %trab_ok, %trc_ok
  br i1 %tr_ok, label %set6, label %t7
set6:
  %s6 = load i16, ptr %status
  %s6a = or i16 %s6, 64
  store i16 %s6a, ptr %status
  br label %t7
t7:

  ; Bit 7: {i8, i8, i8} return — merge(f0,f1) in Lo, f2 anyext in Hi
  %tri8 = call {i8, i8, i8} @make_triple_i8(i8 5, i8 15, i8 25)
  %t8_a = extractvalue {i8, i8, i8} %tri8, 0
  %t8_b = extractvalue {i8, i8, i8} %tri8, 1
  %t8_c = extractvalue {i8, i8, i8} %tri8, 2
  %t8a_ok = icmp eq i8 %t8_a, 5
  %t8b_ok = icmp eq i8 %t8_b, 15
  %t8c_ok = icmp eq i8 %t8_c, 25
  %t8ab_ok = and i1 %t8a_ok, %t8b_ok
  %t8_ok = and i1 %t8ab_ok, %t8c_ok
  br i1 %t8_ok, label %set7, label %done
set7:
  %s7 = load i16, ptr %status
  %s7a = or i16 %s7, 128
  store i16 %s7a, ptr %status
  br label %done
done:

  %result = load i16, ptr %status
  ret i16 %result
}
