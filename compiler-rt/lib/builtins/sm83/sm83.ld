/* SM83 default linker script for lld.
 * Flat memory model: code starts at 0x0000, data follows.
 */
ENTRY(_start)

SECTIONS {
  . = 0x0000;
  .text : { *(.text*) *(.rodata*) }
  .data : { *(.data*) }
  .bss  : {
    __bss_start = .;
    *(.bss*) *(COMMON)
    __bss_end = .;
  }
  __bss_size = __bss_end - __bss_start;

  /DISCARD/ : { *(.comment) *(.eh_frame*) }
}
