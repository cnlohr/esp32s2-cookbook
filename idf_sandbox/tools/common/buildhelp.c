#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../hidapi.h"
#include "../hidapi.c"

#define VID 0x303a
#define PID 0x4004

const char * sym_dump_cmd = "objdump -t %s > build/system_symbols.txt";
const char * compile_cmd = "xtensa-esp32s2-elf-gcc -mlongcalls %s -DHAVE_CONFIG_H  -ffunction-sections -fdata-sections -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -ggdb -O2 -fmacro-prefix-map=/home/cnlohr/git/esp32-c3-playground=. -fmacro-prefix-map=/home/cnlohr/esp/esp-idf=IDF -fstrict-volatile-bitfields -Wno-error=unused-but-set-variable -fno-jump-tables -fno-tree-switch-conversion sandbox.c sandbox.S -T build/sandbox.lds -o build/sandbox.o -nodefaultlibs -nostartfiles";
const char * sym_comp_dump_cmd = "objdump -t build/sandbox.o > build/sandbox_symbols.txt";
const char * ocpy_cmd_inst = "xtensa-esp32s2-elf-objcopy -j .inst -O binary build/sandbox.o build/sandbox_inst.bin";
const char * ocpy_cmd_data = "xtensa-esp32s2-elf-objcopy -j .data -O binary build/sandbox.o build/sandbox_data.bin";

const char * idf_path;
char * extra_cflags;

void appendcflag( const char * flag )
{
	int clen = extra_cflags?strlen( extra_cflags ) : 0;
	int flen = strlen( flag );
	int neln = clen + flen + 2;
	char * newflag = malloc( neln );
	memcpy( newflag, extra_cflags, clen );
	newflag[clen] = ' ';
	memcpy( newflag + clen+1, flag, flen+1 );
	free( extra_cflags );
	extra_cflags = newflag;
}

int main( int argc, char ** argv )
{
	int r;
	int tries = 0;
	uint8_t rdata[512];

	uint32_t allocated_addy = 0;
	uint32_t allocated_size = 0;

	hid_init();
	hid_device * hd = hid_open( VID, PID, L"usbsandbox000" );

	if( !hd )
	{
		fprintf( stderr, "Error: cannot open %04x:%04x\n", VID, PID );
		return -5;
	}

	if( argc != 2 )
	{
		fprintf( stderr, "Error: Usage: [tool] [ESP32 .elf file]\n" );
		return -1;
	}

	idf_path = getenv( "IDF_PATH" );

	if( !idf_path )
	{
		fprintf( stderr, "Error opening IDF path\n" );
		return -5;
	}

	printf( "Using IDF Path: %s\n", idf_path );

	uint32_t advanced_usb_scratch_buffer_address_inst = 0;
	uint32_t advanced_usb_scratch_buffer_address_data = 0;

	do
	{
		printf( "RELOOP\n" );
		// First, get the current size/etc. of the memory allocated for us.
		rdata[0] = 170;
		rdata[1] = 8;
		rdata[2] = 0xff;
		rdata[3] = 0xff;
		rdata[4] = 0xff;
		rdata[5] = 0xff;
		do
		{
			r = hid_send_feature_report( hd, rdata, 65 );
			if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
		} while ( r != 65 );
		tries = 0;
		do
		{
			rdata[0] = 170;
			r = hid_get_feature_report( hd, rdata, sizeof(rdata) );
			if( tries++ > 10 ) { fprintf( stderr, "Error reading feature report on command %d (%d)\n", rdata[1], r ); return -85; }
		} while ( r < 10 );
		allocated_addy = ((uint32_t*)(rdata+0))[0];
		allocated_size = ((uint32_t*)(rdata+0))[1];
		printf( "ALLOCA %p %p\n", allocated_addy, allocated_size );

		if( allocated_addy == 0 )
		{
			// Use a dummy size
			allocated_addy = 0x3ffb0000;
		}

		// Get version
		rdata[0] = 170;
		rdata[1] = 10;
		do
		{
			r = hid_send_feature_report( hd, rdata, 65 );
			if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
		} while ( r != 65 );
		tries = 0;
		do
		{
			rdata[0] = 170;
			r = hid_get_feature_report( hd, rdata, sizeof(rdata) );
			if( tries++ > 10 ) { fprintf( stderr, "Error reading feature report on command %d (%d)\n", rdata[1], r ); return -85; }
		} while ( r < 10 );
		uint32_t vercheck_app_main = ((uint32_t*)(rdata+0))[0];
		uint32_t vercheck_advanced_usb_scratch_buffer_data = ((uint32_t*)(rdata+0))[1];
		uint32_t vercheck_handle_advanced_usb_control_set = ((uint32_t*)(rdata+0))[2];
		uint32_t vercheck_handle_advanced_usb_terminal_get = ((uint32_t*)(rdata+0))[3];

		printf( "Allocated addy: 0x%08x\n", allocated_addy );
		printf( "Allocated size: %d\n", allocated_size );

		advanced_usb_scratch_buffer_address_inst = allocated_addy + 0x70000;
		advanced_usb_scratch_buffer_address_data = allocated_addy;

		{
			char temp[1024];
			if( extra_cflags ) extra_cflags[0] = 0;
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/hal/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/hal/esp32s2/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_wifi/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_event/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/soc/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/soc/esp32s2/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_common/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/driver/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_hw_support/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_rom/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/log/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_system/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/driver/esp32s2/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_ringbuf/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/heap/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_rom/include/esp32s2", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/newlib/platform_include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_timer/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/freertos/esp_additions/include/freertos/", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/freertos/FreeRTOS-Kernel/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/freertos/port/xtensa/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/freertos/FreeRTOS-Kernel/portable/xtensa/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/freertos/include/esp_additions/freertos", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/xtensa/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/xtensa/esp32s2/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/driver/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_lcd/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_lcd/interface", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_hw_support/include/soc", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_hw_support/port", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_hw_support/port/esp32s2/private_include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/hal/platform_port/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/ulp/ulp_riscv/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/ulp/ulp_common/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/hal/esp32s2/include/hal", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_hw_support/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/ulp/ulp_fsm/include", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/esp_hw_support/port/esp32s2", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/ulp/ulp_common/include/esp32s2", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/ulp/ulp_fsm/include/esp32s2", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components/soc/esp32s2/include/soc", idf_path ); appendcflag( temp );
			snprintf( temp, sizeof( temp ) - 1, "-I%s/components", idf_path ); appendcflag( temp );

			appendcflag( "-I." );
			appendcflag( "-I../../components" );
			appendcflag( "-I../../components/hdw-qma6981" );
			appendcflag( "-I../../main/display" );
			appendcflag( "-I../../components/hdw-btn" );
			appendcflag( "-I../../components/hdw-touch" );
			appendcflag( "-I../../components/hdw-led" );
			appendcflag( "-I../../main" );
			appendcflag( "-I../../main/modes" );
			appendcflag( "-I../../build/config" );
		}

		int r;

		{
			char dump_buffer[4096];
			snprintf( dump_buffer, sizeof( dump_buffer) - 1, sym_dump_cmd, argv[1] );

			printf( "# %s\n", dump_buffer );
			r = system( dump_buffer );
			if( r ) { fprintf( stderr, "Error shelling symbols. Error: %d\n", r ); return -6; }
		}

		FILE * provided = fopen( "build/provided.lds", "w" );
		{
			FILE * f = fopen( "build/system_symbols.txt", "r" );
			if( !f || ferror( f ) )
			{
				fprintf( stderr, "Error: could not get symbols\n" );
				return -5;
			}

			char line[1024];

			while( !feof( f ) )
			{
				fgets( line, 1023, f );
				char addy[128], prop[128], V[128], sec[128], size[128], name[1024];
				addy[0] = 0; prop[0] = 0; V[0] = 0; sec[0] = 0; size[0] = 0; name[0] = 0;
				int l = sscanf( line, "%127s %127s %127s %127s %127s %1023s\n", addy, prop, V, sec, size, name );
				int naddy = strtol( addy, 0, 16 );
				if( l == 6 )
				{
					fprintf( provided, "PROVIDE( %s = 0x%s );\n", name, addy );
				}

				// Really bad version match checking.
				if( strcmp( name, "app_main" ) == 0 )
				{
					if( naddy != vercheck_app_main )
					{
						fprintf( stderr, "Error: version mismatch. (%s  %08x != %08x)\n", "app_main", naddy, vercheck_app_main );
						return 5;
					}
				}
				if( strcmp( name, "advanced_usb_scratch_buffer_data" ) == 0 )
				{
					if( naddy != vercheck_advanced_usb_scratch_buffer_data )
					{
						fprintf( stderr, "Error: version mismatch. (%s  %08x != %08x)\n", "advanced_usb_scratch_buffer_data", naddy, vercheck_advanced_usb_scratch_buffer_data );
						return 5;
					}
				}
				if( strcmp( name, "handle_advanced_usb_control_set" ) == 0 )
				{
					if( naddy != vercheck_handle_advanced_usb_control_set )
					{
						fprintf( stderr, "Error: version mismatch. (%s  %08x != %08x)\n", "handle_advanced_usb_control_set", naddy, vercheck_handle_advanced_usb_control_set );
						return 5;
					}
				}
				if( strcmp( name, "handle_advanced_usb_terminal_get" ) == 0 )
				{
					if( naddy != vercheck_handle_advanced_usb_terminal_get )
					{
						fprintf( stderr, "Error: version mismatch. (%s  %08x != %08x)\n", "handle_advanced_usb_terminal_get", naddy, vercheck_handle_advanced_usb_terminal_get );
						return 5;
					}
				}

			}
			fclose( f );
		}
		
		{
			FILE *  f = fopen( "build/ulp_program.map", "r" );
			if( f )
			{
				char line[1024];

				while( !feof( f ) )
				{
					fgets( line, 1023, f );
					char addy[128], prop[128], V[128], sec[128], size[128], name[1024];
					addy[0] = 0; prop[0] = 0; V[0] = 0; sec[0] = 0; size[0] = 0; name[0] = 0;
					int l = sscanf( line, "%127s %127s %127s %127s %127s %1023s\n", addy, prop, V, sec, size, name );
					int naddy = strtol( addy, 0, 16 );
					if( l == 6 && !strstr( sec, "debug" ) && !strchr( name, '.' ) )
					{
						fprintf( provided, "PROVIDE( %s = 0x%08x );\n", name, naddy + 0x50000000 );
					}
				}
				fclose( f );
			}
		}

		fclose( provided );

		{
			FILE * lds = fopen( "build/sandbox.lds", "w" );

			fprintf( lds, 
				"ENTRY(sandbox_main)\n"
				"SECTIONS\n"
				"{\n"
				"	. = 0x%08x;\n"
				"	sandbox_sentinel_start_inst = .;\n"
				"	.inst : ALIGN(4) {\n"
				"		 *(.sandbox.literal)\n"
				"		 *(.sandbox)\n"
				"		 *(.entry.text)\n"
				"		 *(.init.literal)\n"
				"		 *(.init)\n"
				"		 *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.* .iram* .iram1 .iram1.*)\n"
				"		 *(.fini.literal)\n"
				"		 *(.fini)\n"
				"		 *(.gnu.version)\n"
				"	}\n"
				"	sandbox_sentinel_end_inst = .;\n"
				"	. = 0x%08x;\n"
				"	sandbox_sentinel_origin_data = .;\n"
				"	. = 0x%08x + sandbox_sentinel_end_inst - sandbox_sentinel_start_inst;\n"
				"	.data : ALIGN(4) {\n"
				"		sandbox_sentinel_start_data = .;\n"
				"		*(.rodata)\n"
				"		*(.rodata.*)\n"
				"		*(.gnu.linkonce.r.*)\n"
				"		*(.rodata1)\n"
				"		*(.dynsbss)\n"
				"		*(.sbss)\n"
				"		*(.sbss.*)\n"
				"		*(.gnu.linkonce.sb.*)\n"
				"		*(.scommon)\n"
				"		*(.sbss2)\n"
				"		*(.sbss2.*)\n"
				"		*(.gnu.linkonce.sb2.*)\n"
				"		*(.dynbss)\n"
				"		*(.data)\n"
				"		*(.data.*)\n"
				"	}\n"
				"	sandbox_sentinel_end_data = .;\n"
				"	.bss : ALIGN( 4 ) {\n"
				"		*(.bss) /* Tricky: BSS needs to be allocated but not sent. GCC Will not populate these for calculating data size */ \n"
				"		*(.bss.*)\n"
				"	}\n"
				"	sandbox_bss_size = SIZEOF( .bss );\n"
				"}\n"
				"INCLUDE \"build/provided.lds\"\n"
				"INCLUDE \"%s/components/esp_rom/esp32s2/ld/esp32s2.rom.ld\"\n"
				"INCLUDE \"%s/components/soc/esp32s2/ld/esp32s2.peripherals.ld\"\n"
                "INCLUDE \"%s/components/esp_rom/esp32s2/ld/esp32s2.rom.api.ld\"\n"
				, advanced_usb_scratch_buffer_address_inst
				, advanced_usb_scratch_buffer_address_data
				, advanced_usb_scratch_buffer_address_data
				, idf_path
				, idf_path
				, idf_path );

			fclose( lds );
		}

		{
			char compbuf[16384];
			snprintf( compbuf, sizeof(compbuf)-1, compile_cmd, extra_cflags );
			printf( "# %s\n", compbuf );
			r = system( compbuf );
			if( r ) { fprintf( stderr, "Error compiling %d\n", r ); return -6; }
		}

		printf( "# %s\n", sym_comp_dump_cmd );
		r = system( sym_comp_dump_cmd );
		if( r ) { fprintf( stderr, "Error getting symbols for sandbox %d\n", r ); return -6; }

		printf( "# %s\n", ocpy_cmd_inst );
		r = system( ocpy_cmd_inst );
		if( r ) { fprintf( stderr, "Error copying object (inst) %d\n", r ); return -6; }

		printf( "# %s\n", ocpy_cmd_data );
		r = system( ocpy_cmd_data );
		if( r ) { fprintf( stderr, "Error copying object (data) %d\n", r ); return -6; }

		uint32_t data_segment_origin = 0;
		uint32_t data_segment_start = 0;
		uint32_t data_segment_end = 0;
		uint32_t sandbox_bss_size = 0;

		{
			FILE * f = fopen( "build/sandbox_symbols.txt", "r" );
			if( !f || ferror( f ) )
			{
				fprintf( stderr, "Error: could not get symbols\n" );
				return -5;
			}

			char line[1024];

			while( !feof( f ) )
			{
				fgets( line, 1023, f );
				char addy[128], prop[128], V[128], sec[128], size[128], name[1024];
				int l = sscanf( line, "%127s %127s %127s %127s %127s %1023s\n", addy, prop, V, sec, size, name );
				int naddy = strtol( addy, 0, 16 );
				if( l == 5 )
				{
					if( strcmp( size, "sandbox_sentinel_origin_data" ) == 0 )
					{
						data_segment_origin = naddy;
					}
					else if( strcmp( size, "sandbox_sentinel_start_data" ) == 0 )
					{
						data_segment_start = naddy;
					}
					else if( strcmp( size, "sandbox_sentinel_end_data" ) == 0 )
					{
						data_segment_end = naddy;
					}
					else if( strcmp( size, "sandbox_bss_size" ) == 0 )
					{
						sandbox_bss_size = naddy;
					}
				}
			}
			fclose( f );
		}

		int total_segment_size = data_segment_end - data_segment_origin + sandbox_bss_size + 16;
		printf( "Data: %d bytes\n", data_segment_start - data_segment_origin );
		printf( "Data: %d bytes\n", data_segment_end - data_segment_start );
		printf( "BSS: %d\n", sandbox_bss_size );
		printf( "Total Segment Size: %d\n", total_segment_size );

		if( total_segment_size > allocated_size )
		{
			printf( "Doesn't fit in currently allocated room: %d bytes\n", allocated_size );

			// Disable tick function
			rdata[0] = 170;
			rdata[1] = 7;
			rdata[2] = 0;
			rdata[3] = 0>>8;
			rdata[4] = 0>>16;
			rdata[5] = 0>>24;
			do
			{
				r = hid_send_feature_report( hd, rdata, 65 );
				if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
			} while ( r != 65 );
			tries = 0;

			usleep( 20000 );
			
			printf( "Disabled.\n" );
			
			// round up total segment size to a 256-byte boundary (optional)
			// total_segment_size = (total_segment_size+0xff) | 0xffffff00;

			rdata[0] = 170;
			rdata[1] = 8;
			rdata[2] = total_segment_size;
			rdata[3] = total_segment_size>>8;
			rdata[4] = total_segment_size>>16;
			rdata[5] = total_segment_size>>24;
			do
			{
				r = hid_send_feature_report( hd, rdata, 65 );
				if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
			} while ( r != 65 );
			tries = 0;
			printf( "Reallocated to %d\n", total_segment_size );
			continue;
		}
		break;
	} while( 1 );
	return 0;
}

