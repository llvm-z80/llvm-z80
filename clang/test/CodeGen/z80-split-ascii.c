// RUN: %clang_cc1 -triple z80 -S -o - %s | FileCheck %s

// Test that string literals and byte arrays are split into chunks of at most
// 48 bytes per .ascii / .asciz directive, avoiding overly long assembler lines
// that exceed downstream buffer limits (such as z80asm or copt).

// 1. Plain string literal with implicit trailing NUL (70 chars + NUL = 71 bytes).
// Should be split into 48 bytes (.ascii) + 23 bytes including NUL (.asciz).
const char str70[] = "0123456789012345678901234567890123456789012345678901234567890123456789";

// CHECK-LABEL: _str70:
// CHECK-NEXT:  .ascii	"012345678901234567890123456789012345678901234567"
// CHECK-NEXT:  .asciz	"8901234567890123456789"
// CHECK-NEXT:  .size	_str70, 71

// 2. Explicit byte array without trailing NUL (71 bytes).
// Should be split into 48 bytes (.ascii) + 23 bytes (.ascii).
const unsigned char bytes71[71] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
    69, 70, 71
};

// CHECK-LABEL: _bytes71:
// CHECK-NEXT:  .ascii	"\001\002\003\004\005\006\007\b\t\n\013\f\r\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037 !\"#$%&'()*+,-./0"
// CHECK-NEXT:  .ascii	"123456789:;<=>?@ABCDEFG"
// CHECK-NEXT:  .size	_bytes71, 71

// 3. Short string (< 48 bytes) should remain on a single line.
const char str_short[] = "Hello, world!";

// CHECK-LABEL: _str_short:
// CHECK-NEXT:  .asciz	"Hello, world!"
// CHECK-NEXT:  .size	_str_short, 14

// 4. Exact 48 bytes with trailing NUL (47 chars + NUL = 48 bytes).
// Should fit in a single .asciz directive.
const char str48[] = "12345678901234567890123456789012345678901234567";

// CHECK-LABEL: _str48:
// CHECK-NEXT:  .asciz	"12345678901234567890123456789012345678901234567"
// CHECK-NEXT:  .size	_str48, 48

// 5. Exact 48 bytes without trailing NUL.
// Should fit in a single .ascii directive.
const char bytes48[48] = "123456789012345678901234567890123456789012345678";

// CHECK-LABEL: _bytes48:
// CHECK-NEXT:  .ascii	"123456789012345678901234567890123456789012345678"
// CHECK-NEXT:  .size	_bytes48, 48
