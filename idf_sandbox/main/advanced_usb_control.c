#include "advanced_usb_control.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include <stdio.h>
#include "esp_partition.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "esp_log.h"
#include "hal/usb_ll.h"
#include "rom/cache.h"
#include "soc/sensitive_reg.h"
#include "soc/dport_access.h"
//#include "soc/rtc_wdt.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"  // for WRITE_PERI_REG
#include <esp_heap_caps.h>

#include "esp_flash.h"

// Uncomment the ESP_LOGI to activate logging for this file.
// Logging can cause issues in operation, so by default it should remain off.
#define ULOG( x... )  \
	//printf( x );

uint32_t * advanced_usb_scratch_buffer_data;
uint32_t   advanced_usb_scratch_buffer_data_size;
uint32_t   advanced_usb_scratch_immediate[SCRATCH_IMMEDIATE_DWORDS];
#define AUPB_SIZE 2048
uint8_t* advanced_usb_printf_buffer = NULL;
int	  advanced_usb_printf_head, advanced_usb_printf_tail;

uint32_t * advanced_usb_read_offset;
static uint8_t did_init_flash_function;

/**
 * @brief Accept a "get" feature report command from a USB host and write
 *		 back whatever is needed to send back.
 * 
 * @param datalen Number of bytes host is requesting from us.
 * @param data Pointer to a feature get request for the command set.
 * @return Number of bytes that will be returned.
 */
int handle_advanced_usb_control_get( int reqlen, uint8_t * data )
{
	if( advanced_usb_read_offset == 0 ) return 0;
	memcpy( data, advanced_usb_read_offset, reqlen );
	return reqlen;
}

/**
 * @brief Internal function for writing log data to the ring buffer.
 * 
 * @param cookie *unused*
 * @param data Pointer to text that needs to be logged.
 * @return size Number of bytes in data that need to be logged.
 */
static int advanced_usb_write_log( void* cookie, const char* data, int size )
{
	if(NULL == advanced_usb_printf_buffer)
	{
		advanced_usb_printf_buffer = heap_caps_calloc(AUPB_SIZE, sizeof(uint8_t), /*MALLOC_CAP_SPIRAM*/ MALLOC_CAP_DEFAULT );
	}

	int next = ( advanced_usb_printf_head + 1 ) % AUPB_SIZE;
	int idx = 0;
	cookie = cookie; // unused
	// Drop extra characters on the floor.
	while( next != advanced_usb_printf_tail && idx < size )
	{
		advanced_usb_printf_buffer[advanced_usb_printf_head] = data[idx++];
		advanced_usb_printf_head = next;
		next = ( advanced_usb_printf_head + 1 ) % AUPB_SIZE;
	}
	return size;
}

/**
 * @brief vaprintf standin for USB logging.
 * 
 * @param fmt vaprintf format
 * @param args vaprintf args
 * @return size Number of characters that were written.
 */
int advanced_usb_write_log_printf(const char *fmt, va_list args)
{
	char buffer[512];
	int l = vsnprintf( buffer, 511, fmt, args );
	advanced_usb_write_log( 0, buffer, l );
	return l;
}

/**
 * @brief vaprintf standin for USB logging.
 * 
 * @param fmt vaprintf format
 * @param args vaprintf args
 * @return size Number of characters that were written.
 */
int uprintf( const char * fmt, ... )
{
	va_list args;
	va_start(args, fmt);
	int r = advanced_usb_write_log_printf(fmt, args);
	va_end(args);
	return r;
}



/**
 * @brief USB request to get text in buffer
 * 
 * @param reqlen The number of bytes the host is requesting from us.
 * @param data The data that we will write back into
 * @return size Number of bytes to be returned to the host.
 */
int handle_advanced_usb_terminal_get( int reqlen, uint8_t * data )
{
	if(NULL == advanced_usb_printf_buffer)
	{
		advanced_usb_printf_buffer = heap_caps_calloc(AUPB_SIZE, sizeof(uint8_t), /*MALLOC_CAP_SPIRAM*/ MALLOC_CAP_DEFAULT );
	}

	int togo = ( advanced_usb_printf_head - advanced_usb_printf_tail + AUPB_SIZE ) % AUPB_SIZE;

	data[0] = 171;

	int mark = 1;
	if( togo )
	{
		if( togo > reqlen-2 ) togo = reqlen-2;
		while( mark <= togo )
		{
			data[++mark] = advanced_usb_printf_buffer[advanced_usb_printf_tail++];
			if( advanced_usb_printf_tail == AUPB_SIZE )
				advanced_usb_printf_tail = 0;
		}
	}
	return mark+1;
}

/**
 * @brief Accept a "send" feature report command from a USB host and interpret it.
 *		 executing whatever needs to be executed.
 * 
 * @param datalen Total length of the buffer (command ID incldued)
 * @param data Pointer to full command
 * @return A pointer to a null terminated JSON string. May be NULL if the load
 */
void IRAM_ATTR handle_advanced_usb_control_set( int datalen, const uint8_t * data )
{
	if( datalen < 6 ) return;
	intptr_t value = data[2] | ( data[3] << 8 ) | ( data[4] << 16 ) | ( data[5]<<24 );
	switch( data[1] )
	{
	case AUSB_CMD_REBOOT:
		{
		// Drive USB Lines to force an enumeration
		usb_ll_int_phy_pullup_conf(false, true, false, true);

		// Wait 600 ms (to emulate a POR, a shorter time probably works, but is untested)
		vTaskDelay(600 / portTICK_PERIOD_MS);

		// Decide to reboot into bootloader or not.
		REG_WRITE(RTC_CNTL_OPTION1_REG, value?RTC_CNTL_FORCE_DOWNLOAD_BOOT:0 );

#ifdef IDF_VER
		// Restart via the ESP-IDF API
		// This allows a clean shutdown to happen via the esp_register_shutdown_handler()
		esp_restart();
#else
		void software_reset(uint32_t x);
		software_reset( 0 ); // ROM function
#endif

		break;
		}
	case AUSB_CMD_WRITE_RAM:
		{
			// Write into scratch.
			if( datalen < 8 ) return;
			intptr_t length = data[6] | ( data[7] << 8 );
			ULOG( "Writing %d into %p\n", length, (void*)value );
			memcpy( (void*)value, data+8, length );
			break;
		}
	case AUSB_CMD_READ_RAM:
		// Configure read.
		advanced_usb_read_offset = (uint32_t*)value;
		break;
	case AUSB_CMD_EXEC_RAM:
		// Execute scratch
		{
			void (*scratchfn)() = (void (*)())(value);
			ULOG( "Executing %p (%p) // base %08x/%p\n", (void*)value, scratchfn, 0, advanced_usb_scratch_buffer_data );
			scratchfn();
		}
		break;
	case AUSB_CMD_SWITCH_MODE:
		// Switch Swadge Mode
		{
			if( g_SandboxStruct && g_SandboxStruct->fnDecom )
			{
				g_SandboxStruct->fnDecom();
			}
			ULOG( "SwadgeMode Value: 0x%08x\n", value );
			g_SandboxStruct = (struct SandboxStruct *) value;
		}
		break;
	case AUSB_CMD_ALLOC_SCRATCH:
		// (re) allocate the primary scratch buffer.
		{
			// value = -1 will just cause a report.
			// value = 0 will clear it.
			// value < 0 just read current data.
			
			ULOG( "Allocating to %d (Current: %08x / %d)\n", value, (unsigned int)advanced_usb_scratch_buffer_data, (int)advanced_usb_scratch_buffer_data_size );
			if( ! (value & 0x80000000 ) )
			{
				if( value > advanced_usb_scratch_buffer_data_size  )
				{
					advanced_usb_scratch_buffer_data = realloc( advanced_usb_scratch_buffer_data, value );
					memset( advanced_usb_scratch_buffer_data, 0, value );
					advanced_usb_scratch_buffer_data_size = value;
				}
				if( value == 0 )
				{
					if( advanced_usb_scratch_buffer_data ) free( advanced_usb_scratch_buffer_data );
					advanced_usb_scratch_buffer_data = 0;
					advanced_usb_scratch_buffer_data_size = 0;
				}
			}
			advanced_usb_scratch_immediate[0] = (intptr_t)advanced_usb_scratch_buffer_data;
			advanced_usb_scratch_immediate[1] = advanced_usb_scratch_buffer_data_size;
			advanced_usb_read_offset = (uint32_t*)(&advanced_usb_scratch_immediate[0]);
			ULOG( "New: %08x / %d\n", (unsigned int)advanced_usb_scratch_buffer_data, (int)advanced_usb_scratch_buffer_data_size );
		}
		break;
	case ACMD_CMD_MEMSET:
	{
		if( datalen < 11 ) return;
		intptr_t length = data[6] | ( data[7] << 8 ) | ( data[8] << 16 ) | ( data[9] << 24 );
		ULOG( "Memset %d into %p\n", length, (void*)value );
		memset( (void*)value, data[10], length );
		break;
	}
	case ACMD_CMD_GETVER:
	{
		// TODO: This is terrible.  It should be improved.  It's just a thing to try to version control the on-device map with the on-host map.
		void app_main(void);
		advanced_usb_scratch_immediate[0] = (uint32_t)&app_main;
		advanced_usb_scratch_immediate[1] = (uint32_t)&advanced_usb_scratch_buffer_data;
		advanced_usb_scratch_immediate[2] = (uint32_t)&handle_advanced_usb_control_set;
		advanced_usb_scratch_immediate[3] = (uint32_t)&handle_advanced_usb_terminal_get;
		advanced_usb_read_offset = advanced_usb_scratch_immediate;
		break;
	}
	case AUSB_CMD_FLASH_ERASE: // Flash erase region
	{
		if( datalen < 10 ) return ;
		intptr_t length = data[6] | ( data[7] << 8 ) | ( data[8] << 16 ) | ( data[9]<<24 );
		if( !did_init_flash_function )
			esp_flash_init( 0 );
		if( ( length & 0x80000000 ) && value == 0 )
			esp_flash_erase_chip( 0 );
		else
			esp_flash_erase_region( 0, value, length );	
		break;
	}
	case AUSB_CMD_FLASH_WRITE: // Flash write region
	{
		if( datalen < 8 ) return;
		intptr_t length = data[6] | ( data[7] << 8 );
		esp_flash_write( 0, data+8, value, length );
		break;
	}
	case AUSB_CMD_FLASH_READ: // Flash read region
	{
		if( datalen < 8 ) return ;
		intptr_t length = data[6] | ( data[7] << 8 );
		if( length > sizeof( advanced_usb_scratch_immediate ) )
			length = sizeof( advanced_usb_scratch_immediate );
		esp_flash_read( 0, advanced_usb_scratch_immediate, value, length );
		advanced_usb_read_offset = advanced_usb_scratch_immediate;
		break;
	}
	}
}

