Minimal declaration-only headers for the torture suite.

Clang ships the freestanding headers (`stdarg.h`, `limits.h`, `stdint.h`,
`stddef.h`, `float.h`, `stdbool.h`) but not the hosted ones. Of the 1879
execute tests only 299 include anything at all, and the hosted headers they
reach for are `stdio.h` (45), `stdlib.h` (40), `string.h` (25) and `math.h` (3).

These headers declare those functions and define nothing. A test that only
needs `memcpy` or `strlen` then links against compiler-rt and runs; a test that
needs `printf` fails at the *link* step with a clear undefined symbol instead of
dying in the preprocessor with "file not found". That distinction is what makes
`skip=libc` an accurate classification rather than a guess.
