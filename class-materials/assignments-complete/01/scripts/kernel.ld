OUTPUT_ARCH( "riscv" )
ENTRY( entry )

SECTIONS
{
  /*
   * ensure that entry.S / entry is at 0x80000000,
   * where qemu's -kernel jumps.
   */
  . = 0x80000000;

  .text : {
    *(.text)
  }
/* BEGIN DELETE BLOCK */
  .stack : {
    . = ALIGN(16);
    _stack_start = .;
    . += 0x1000;
    PROVIDE(stack0 = .);
  }
/* END DELETE BLOCK */
}
