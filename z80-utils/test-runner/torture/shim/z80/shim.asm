; Runtime shim for the GCC C torture execute tests (Z80).
;
; The tests are self-checking: main() returns 0 on success and calls abort()
; on failure. The harness crt0 records main's return value at _exitcode and
; ends execution at _halt, so the shim only has to steer abort()/exit() into
; that same exit point with the right value:
;
;   main() returns 0   ->  _exitcode = 0x0000   pass
;   abort()            ->  _exitcode = 0xDEAD   failed, and specifically by aborting
;   exit(n)            ->  _exitcode = n
;
; abort() and exit() jump straight to _halt, past the store crt0 does after
; main returns, so they have to write _exitcode themselves.
;
; Z80 (__sdcccall(1)) passes the first 16-bit argument in HL and returns a
; 16-bit value in DE.

	.area _CODE
	.globl _abort
	.globl _exit
	.globl _link_error
	.globl _halt
	.globl _exitcode

; link_error() and link_error0..7 are deliberately NOT defined here.  They mark
; a call the optimizer was supposed to delete, and the resulting undefined
; symbol is the test's verdict: the runner reports it as OPTIM.  Defining them
; would let a test whose marker survived but went unexecuted report a pass.
_abort:
	ld	de,#0xDEAD
	ld	(_exitcode),de
	jp	_halt

_exit:
	ex	de,hl
	ld	(_exitcode),de
	jp	_halt
