/* Minimal repro for the elf2rel .bss-materialization bug.
 * `buf` is uninitialized -> lands in ELF .bss (SHT_NOBITS, zero size on disk).
 * `code` is the only "real" content.
 */
char buf[4096];

int code(void) {
    return buf[0];
}
