MEMORY {
  /* NXP LPC810 */
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00001000 /* 4 Kbytes */
  RAM (rwx) :  ORIGIN = 0x10000000, LENGTH = 0x00000400 /* 1 Kbyte */
}

SECTIONS {
  .text : {
    _text = .;
    KEEP(*(.isr_vector))
    KEEP(*(.after_vectors))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .data : AT (_etext) {
    _data = .;
    *(vtable)
    *(.data*)
    _edata = .;
  } > RAM

  .bss (NOLOAD) : {
    _bss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM

  .ARM.exidx (NOLOAD) : {
    /* No idea why this prevents building */
  }
}
