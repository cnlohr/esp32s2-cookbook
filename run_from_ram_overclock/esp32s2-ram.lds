/* Based on a few things, including app_32s2.ld, but notably, CPQ's MDK https://github.com/cpq/mdk */
/* This works both for RAM and for flash */

MEMORY {
  drom   (rw)   : ORIGIN = 0X3ffa0000, LENGTH = 64k
  irom0   (rwx)  : ORIGIN = 0x40000000, LENGTH = 64k
  irom1   (rwx)  : ORIGIN = 0x40010000, LENGTH = 64k

  cache0 (rw)  : ORIGIN = 0x40020000, LENGTH = 32k
  cache1 (rw)  : ORIGIN = 0x3ffb0000, LENGTH = 32k
  iram   (rwx)  : ORIGIN = 0x40030000, LENGTH = 256k  /* First section has bootloader */
  dram   (rw)   : ORIGIN = 0x3ffc0000, LENGTH = 256k

  drtcfast (rw)   : ORIGIN = 0x3ff9E000, LENGTH = 8k
  irtcfast (rwx)   : ORIGIN = 0x40070000, LENGTH = 8k
  rtcslow (rwx)   : ORIGIN = 0x50000000, LENGTH = 8k

  iflash (rwx)  : ORIGIN = 0x3F000000, LENGTH = 4096k
  dflashi (rwx)  : ORIGIN = 0x3F500000, LENGTH = 10752k
  dflashd (rwx)  : ORIGIN = 0x40080000, LENGTH = 7680k
}

/* 0x40030000, 0x3ffc0000 */


_eram = ORIGIN(dram) + LENGTH(dram);
ENTRY(ram_main)

SECTIONS {
	/* .vectors  : { KEEP(*(.vectors))   } > iram */
	.text     : {
		*(.initial_jump)
		*(.entry.text)
		*(.init.literal)
		*(.init)
		*(.literal .text .literal.* .text.* .stub)
		*(.out_jump.literal.*)
		*(.out_jump.*)
		. = ALIGN(4);
	} > iram AT > dram

	.data : {
		. = ALIGN(4);
		_sdata = .;
		*(.data)
		*(.data*)
		*(.rodata)
		*(.rodata*)
		*(.gnu.linkonce.r.*)
		*(.rodata1)
		. = ALIGN(4);
		_edata = .;
	} > dram

	.bss : {
		. = ALIGN(4);
		_sbss = .;
		*(.bss)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > dram

	. = ALIGN(4);
	PROVIDE(end = .);
	PROVIDE(_end = .);

	/*
	/DISCARD/ : { *(.debug) *(.debug*) *(.xtensa.*) *(.comment) }
	*/
}




INCLUDE "/home/cnlohr/esp/esp-idf/components/esp_rom/esp32s2/ld/esp32s2.rom.ld"
INCLUDE "/home/cnlohr/esp/esp-idf/components/soc/esp32s2/ld/esp32s2.peripherals.ld"
INCLUDE "/home/cnlohr/esp/esp-idf/components/esp_rom/esp32s2/ld/esp32s2.rom.api.ld"
