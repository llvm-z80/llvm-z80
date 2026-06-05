; Test 26: fixed-point arithmetic
; Tests G_SMULFIX, G_UMULFIX, G_SDIVFIX, G_UDIVFIX with various scales.
; Uses 8.8 fixed-point format (scale=8): 1.0 = 256, 0.5 = 128.
; expect 0x000F (4 bits, all pass)

define void @_start() {
  call void asm sideeffect "ld sp, #0xFFFE", ""()
  %r = call i16 @main()
  call void asm sideeffect ".globl _halt\0A_halt:\0Ahalt", ""()
  ret void
}

declare i16 @llvm.smul.fix.i16(i16, i16, i32)
declare i16 @llvm.umul.fix.i16(i16, i16, i32)
declare i16 @llvm.sdiv.fix.i16(i16, i16, i32)
declare i16 @llvm.udiv.fix.i16(i16, i16, i32)

define i16 @main() {
  %status = alloca i16
  store i16 0, ptr %status

  ; Bit 0: smulfix 2.0 * 3.0 = 6.0 (512 * 768 >> 8 = 1536)
  %sm = call i16 @llvm.smul.fix.i16(i16 512, i16 768, i32 8)
  %sm_ok = icmp eq i16 %sm, 1536
  br i1 %sm_ok, label %set0, label %t1
set0:
  %s0 = load i16, ptr %status
  %s0a = or i16 %s0, 1
  store i16 %s0a, ptr %status
  br label %t1
t1:

  ; Bit 1: umulfix 1.5 * 2.0 = 3.0 (384 * 512 >> 8 = 768)
  %um = call i16 @llvm.umul.fix.i16(i16 384, i16 512, i32 8)
  %um_ok = icmp eq i16 %um, 768
  br i1 %um_ok, label %set1, label %t2
set1:
  %s1 = load i16, ptr %status
  %s1a = or i16 %s1, 2
  store i16 %s1a, ptr %status
  br label %t2
t2:

  ; Bit 2: sdivfix 6.0 / 2.0 = 3.0 ((1536 << 8) / 512 = 768)
  %sd = call i16 @llvm.sdiv.fix.i16(i16 1536, i16 512, i32 8)
  %sd_ok = icmp eq i16 %sd, 768
  br i1 %sd_ok, label %set2, label %t3
set2:
  %s2 = load i16, ptr %status
  %s2a = or i16 %s2, 4
  store i16 %s2a, ptr %status
  br label %t3
t3:

  ; Bit 3: udivfix 3.0 / 1.5 = 2.0 ((768 << 8) / 384 = 512)
  %ud = call i16 @llvm.udiv.fix.i16(i16 768, i16 384, i32 8)
  %ud_ok = icmp eq i16 %ud, 512
  br i1 %ud_ok, label %set3, label %done
set3:
  %s3 = load i16, ptr %status
  %s3a = or i16 %s3, 8
  store i16 %s3a, ptr %status
  br label %done
done:

  %result = load i16, ptr %status
  ret i16 %result
}
