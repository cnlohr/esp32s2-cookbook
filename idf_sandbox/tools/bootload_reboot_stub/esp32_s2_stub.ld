ENTRY(bootload_reboot_stub)
SECTIONS
{
	. = 0x4006ffe0;
	.inst : ALIGN(4) {

		/* esptool.py header */

		LONG( 0x000001e9 )  /* Magic + we want to have a "jump to" location once we write */
		LONG( bootload_reboot_stub )
		LONG( 0x00000000 )
		LONG( 0x02000002 )
		LONG( 0x000000e9 )
		LONG( 0x00000000 )
		LONG( __IMAGE_BEGIN__ )
		LONG( ( __IMAGE_END__ - __IMAGE_BEGIN__ ) )

		/* The actual firmware image */

		__IMAGE_BEGIN__ = .;
		*(.initial_jump)
		*(.entry.text)
		*(.init.literal)
		*(.init)
		*(.literal .text .literal.* .text.* .stub)
		*(.out_jump.literal.*)
		*(.out_jump.*)
		__IMAGE_END__ = .;

		BYTE( 0 ) /* There's a janky edge case with esptool.py that this solves */
	}
	.checksum : ALIGN(16) {
		BYTE( 0 )
	}
}
PROVIDE ( chip_usb_set_persist_flags = 0x400135e8 );
PROVIDE ( software_reset = 0x40010068 );
PROVIDE ( software_reset_cpu = 0x40010080 );

