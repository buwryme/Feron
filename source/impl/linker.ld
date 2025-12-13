/* source/impl/linker.ld
   Place the multiboot header at the start of the file so GRUB finds it
   and place the kernel code at 0x100000 (virtual address) for execution.
*/

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
  /* Put the multiboot header at the very start of the file (file offset 0).
     This ensures GRUB can find the header within the first 8 KiB of the file. */
  .multiboot ALIGN(8) :
  {
    KEEP(*(.multiboot))
  }

  /* Now set the virtual origin for the kernel proper (place in memory at 1 MiB).
     The file will contain the multiboot header first, then the rest of the sections
     will be placed at their virtual addresses (with padding in the file). */
  . = 0x100000;

  .text ALIGN(4096) :
  {
    *(.text*)
    *(.rodata*)
  } :text

  .data ALIGN(4096) :
  {
    *(.data*)
  } :data

  .bss (NOLOAD) : ALIGN(4096)
  {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  } :bss

  /* final alignment and end symbol */
  . = ALIGN(8);
  _end = .;
  PROVIDE(_end = .);
}
