/* Based on a few things, including app_32s2.ld, but notably, CPQ's MDK https://github.com/cpq/mdk */
/* This works both for RAM and for flash */

MEMORY {
  drom   (rw)    : ORIGIN = 0x3ff00000, LENGTH = 128k
  irom0   (rwx)  : ORIGIN = 0x40000000, LENGTH = 384k

  iram   (rwx)   : ORIGIN = 0x40370000, LENGTH = 448k  /* First section has bootloader */
  dram   (rw)    : ORIGIN = 0x3fc88000, LENGTH = 480k

  extiram (rwx)  : ORIGIN = 0x3c000000, LENGTH = 32768k
  extdram (rw)   : ORIGIN = 0x42000000, LENGTH = 32768k

  rtcfast (rwx)  : ORIGIN = 0x600fe000, LENGTH = 8k
  rtcslow (rwx)  : ORIGIN = 0x50000000, LENGTH = 8k
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




INCLUDE "/home/cnlohr/esp/esp-idf/components/esp_rom/esp32s3/ld/esp32s3.rom.ld"
INCLUDE "/home/cnlohr/esp/esp-idf/components/soc/esp32s3/ld/esp32s3.peripherals.ld"
INCLUDE "/home/cnlohr/esp/esp-idf/components/esp_rom/esp32s3/ld/esp32s3.rom.api.ld"
