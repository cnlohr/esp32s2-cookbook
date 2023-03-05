#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "advanced_usb_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/dedic_gpio_reg.h"
#include "soc/dport_access.h"
#include "soc/gpio_sig_map.h"
#include "soc/rtc.h"
#include "freertos/portmacro.h"

#define DisableISR()            do { XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL); portbenchmarkINTERRUPT_DISABLE(); } while (0)
#define EnableISR()             do { portbenchmarkINTERRUPT_RESTORE(0); XTOS_SET_INTLEVEL(0); } while (0)

#define MAX_IN_TIMEOUT 1000
#include "ch32v003_swio.h"

uint32_t t1coeff;
uint32_t pinmask, pinmaskpower;
uint8_t retbuff[256];
uint8_t * retbuffptr = 0;

void sandbox_main()
{
	REG_WRITE( IO_MUX_GPIO6_REG, 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.  Optional: 10k pull-up resistor.
	REG_WRITE( IO_MUX_GPIO7_REG, 1<<FUN_IE_S | 1<<FUN_PU_S | 3<<FUN_DRV_S );  //VCC for part 40mA drive.

	retbuffptr = retbuff;

	pinmask = 1<<6;
	pinmaskpower = 1<<7;

	GPIO.out_w1ts = pinmaskpower;
	GPIO.enable_w1ts = pinmaskpower;
	GPIO.out_w1ts = pinmask;
	GPIO.enable_w1ts = pinmask;

	esp_rom_delay_us(5000);

	rtc_cpu_freq_config_t m;
	rtc_clk_cpu_freq_get_config( &m );
	switch( m.freq_mhz )
	{
	case 240:
		t1coeff = 9; // 9 or 10 is good.  5 is too low. 13 is sometimes too high.
		break;
	default:
		t1coeff = 100; // Untested At Other Speeds
		break;
	}


#if 0
//	DoSongAndDanceToEnterPgmMode( t1coeff, pinmask );
	SendWord32( t1coeff, pinmask, 0x7e, 0x5aa50000 | (1<<10) ); // Shadow Config Reg
	SendWord32( t1coeff, pinmask, 0x7d, 0x5aa50000 | (1<<10) ); // CFGR (1<<10 == Allow output from slave)

	uint32_t rval = 0;
	int r = ReadWord32( t1coeff, pinmask, 0x7c, &rval ); // Capability Register (CPBR)
	uprintf( "CPBR: %d - %08x %08x\n", r, rval, REG_READ( GPIO_IN_REG ) );
	
	#if 0
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Make the debug module work properly.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Initiate a halt request.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 1 ); // Clear halt request bit.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x40000001 ); // Resume
	#endif
	
	r = ReadWord32( t1coeff, pinmask, 0x11, &rval ); // 
	uprintf( "DMSTATUS: %d - %08x %08x\n", r, rval, REG_READ( GPIO_IN_REG ) );
#endif

}

void teardown()
{
	// Power-Down
	GPIO.out_w1tc = 1<<6;
	GPIO.out_w1ts = 1<<6;
	GPIO.out_w1tc = 1<<7;
}

void sandbox_tick()
{
/*
	uint32_t rval = 0;
	SendWord32( t1coeff, pinmask, 0x7e, 0x5aa50000 | (1<<10) ); // Shadow Config Reg
	SendWord32( t1coeff, pinmask, 0x7d, 0x5aa50000 | (1<<10) ); // CFGR (1<<10 == Allow output from slave)
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Make the debug module work properly.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Initiate a halt request.

//	esp_rom_delay_us(100);
//	r = ReadWord32( t1coeff, pinmask, CDATA0, &rval );
//	uprintf( "DMSTATUS: %d - %08x\n", r, rval );
	r = ReadWord32( t1coeff, pinmask, CDMSTATUS, &rval );
//	uprintf( "DMSTATUS: %d - %08x\n", r, rval );

//	SendWord32( t1coeff, pinmask, CDMCONTROL, 1 ); // Clear halt request bit.
//	r = ReadWord32( t1coeff, pinmask, 0x05, &rval ); // 
//	uprintf( "DMSTATUS: %d - %08x\n", r, rval );
//	SendWord32( t1coeff, pinmask, CDATA0, 0x12349999 ); // Reset Debug Subsystem
//	uprintf( "DMSTATUS: %d - %08x\n", r, rval );
//	SendWord32( t1coeff, pinmask, 0x05, 0x789a4444 ); // Reset Debug Subsystem
//	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x00000001 ); // Initiate a halt request.
*/
	//esp_rom_delay_us(100);
}

int ch32v003_usb_feature_report( uint8_t * buffer, int reqlen, int is_get )
{
	if( is_get )
	{
		int len = retbuffptr - retbuff;
		buffer[0] = len;
		if( len > reqlen-1 ) len = reqlen-1;
		memcpy( buffer+1, retbuff, len );
		retbuffptr = retbuff;
		return len+1;
	}
	
	// Is send.
	// buffer[0] is the request ID.
	uint8_t * iptr = &buffer[1];
	while( iptr - buffer < reqlen )
	{
		uint8_t cmd = *(iptr++);
		int remain = reqlen - (iptr - buffer);
		if( cmd == 0xfe ) // We will never write to 0x7f.
		{
			cmd = *(iptr++);
			switch( cmd )
			{
			case 0x01:
				DoSongAndDanceToEnterPgmMode( t1coeff, pinmask );
				break;
			case 0x02: // Power-down 
				uprintf( "Power down\n" );
				GPIO.out_w1tc = pinmaskpower;
				GPIO.enable_w1ts = pinmaskpower;
				GPIO.out_w1tc = pinmask;
				GPIO.enable_w1ts = pinmask;
				break;
			case 0x03: // Power-up
				GPIO.out_w1ts = pinmaskpower;
				GPIO.enable_w1ts = pinmaskpower;
				GPIO.out_w1ts = pinmask;
				GPIO.enable_w1ts = pinmask;
				break;
			case 0x04: // Delay( uint16_t us )
				esp_rom_delay_us(iptr[0] | (iptr[1]<<8) );
				iptr += 2;
				break;
			}
		} else if( cmd == 0xff )
		{
			break;
		}
		else
		{
			// Otherwise it's a regular command.
			// 7-bit-cmd .. 1-bit read(0) or write(1) 
			// if command lines up to a normal QingKeV2 debug command, treat it as that command.

			if( cmd & 1 )
			{
				if( remain >= 4 )
				{
					SendWord32( t1coeff, pinmask, cmd>>1, iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24) );
					iptr += 4;
				}
			}
			else
			{
				if( remain >= 1 && (sizeof(retbuff)-(retbuffptr - retbuff)) >= 4 )
				{
					int r = ReadWord32( t1coeff, pinmask, cmd>>1, (uint32_t*)&retbuffptr[1] );
					retbuffptr[0] = r;
					if( r < 0 )
						*((uint32_t*)&retbuffptr[1]) = 0;
					retbuffptr += 5;
				}
			}
		}
	}

	return 0;
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnDecom = teardown,
	.fnAdvancedUSB = ch32v003_usb_feature_report
};
