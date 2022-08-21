/* Based on a few things, including app_32s2.ld */

ENTRY(ram_main)
SECTIONS
{
	/* We start with our IRAM at 0x4002_8000 (which mirors) 0x3FFB_8000; This provides room for the header. 
		But we bump it up a little, the software loader is resident 0x40028000-0x40029028.
		We do 0x40030000, 0x3ffc0000.  This is neat because it also provides access to DRAM.
	*/
	. = 0x40030000 - 0x20;

	.inst : ALIGN(4) {

		/* esptool.py header */

		LONG( 0x000001e9 )  /* Magic + we want to have a "jump to" location once we write */
		LONG( ram_main )
		LONG( 0x00000000 )
		LONG( 0x02000002 )
		LONG( 0x000000e9 )
		LONG( 0x00000000 )
		LONG( __CODE_BEGIN__ )
		LONG( ( __CODE_END__ - __CODE_BEGIN__ + __DATA_END__ - __DATA_BEGIN__ ) )

		__CODE_BEGIN__ = .;
		sandbox_sentinel_start_inst = .;
		*(.initial_jump)
		*(.entry.text)
		*(.init.literal)
		*(.init)
		*(.literal .text .literal.* .text.* .stub)
		*(.out_jump.literal.*)
		*(.out_jump.*)
		. = ALIGN(16);
		__CODE_END__ = .;
	}

	. = 0x3ffc0000;
	sandbox_sentinel_origin_data = .;
	. = 0x3ffc0000 + __CODE_END__ - __CODE_BEGIN__;

	.data : ALIGN(16) {
		__DATA_BEGIN__ = .;
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		*(.rodata1)
		*(.dynsbss)
		*(.sbss)
		*(.sbss.*)
		*(.gnu.linkonce.sb.*)
		*(.scommon)
		*(.sbss2)
		*(.sbss2.*)
		*(.gnu.linkonce.sb2.*)
		*(.dynbss)
		*(.data)
		*(.data.*)
		__DATA_END__ = .;

		/* For the fake checksum */
		BYTE( 0 )
		. = ALIGN( 16 );
		BYTE( 0 )
	}

/*
	sandbox_sentinel_end_inst = .;
	. = 0x3ffeabc4;
	sandbox_sentinel_origin_data = .;
	. = 0x3ffeabc4 + sandbox_sentinel_end_inst - sandbox_sentinel_start_inst;



	.data : ALIGN(4) {
		sandbox_sentinel_start_data = .;
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		*(.rodata1)
		*(.dynsbss)
		*(.sbss)
		*(.sbss.*)
		*(.gnu.linkonce.sb.*)
		*(.scommon)
		*(.sbss2)
		*(.sbss2.*)
		*(.gnu.linkonce.sb2.*)
		*(.dynbss)
		*(.data)
		*(.data.*)
		__IMAGE_END__ = .;

		BYTE( 0 ) // There's a janky edge case with esptool.py that this solves 

	}

	.checksum : ALIGN(16) {
		BYTE( 0 )
	}
*/

/*
	sandbox_sentinel_start_inst = .;
	.inst : ALIGN(4) {
		*(.entry.text)
		*(.init.literal)
		*(.init)
		*(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.* .iram* .iram1 .iram1.*)
		*(.fini.literal)
		*(.fini)
		*(.gnu.version)

		sandbox_sentinel_start_data = .;
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		*(.rodata1)
		*(.dynsbss)
		*(.sbss)
		*(.sbss.*)
		*(.gnu.linkonce.sb.*)
		*(.scommon)
		*(.sbss2)
		*(.sbss2.*)
		*(.gnu.linkonce.sb2.*)
		*(.dynbss)
		*(.data)
		*(.data.*)
	}
	sandbox_sentinel_end_inst = .;
*/
/*
	. = 0x3ffe1000;
	sandbox_sentinel_origin_data = .;
	. = 0x3ffe1000 + sandbox_sentinel_end_inst - sandbox_sentinel_start_inst;
*/
/*
	.data : ALIGN(4) {
		sandbox_sentinel_start_data = .;
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r.*)
		*(.rodata1)
		*(.dynsbss)
		*(.sbss)
		*(.sbss.*)
		*(.gnu.linkonce.sb.*)
		*(.scommon)
		*(.sbss2)
		*(.sbss2.*)
		*(.gnu.linkonce.sb2.*)
		*(.dynbss)
		*(.data)
		*(.data.*)
	}
	sandbox_sentinel_end_data = .;
	.bss : ALIGN( 4 ) {
		*(.bss) // Tricky: BSS needs to be allocated but not sent. GCC Will not populate these for calculating data size
		*(.bss.*)
	}
*/
	sandbox_bss_size = SIZEOF( .bss );
}

INCLUDE "/home/cnlohr/esp/esp-idf/components/esp_rom/esp32s2/ld/esp32s2.rom.ld"
INCLUDE "/home/cnlohr/esp/esp-idf/components/soc/esp32s2/ld/esp32s2.peripherals.ld"
INCLUDE "/home/cnlohr/esp/esp-idf/components/esp_rom/esp32s2/ld/esp32s2.rom.api.ld"
