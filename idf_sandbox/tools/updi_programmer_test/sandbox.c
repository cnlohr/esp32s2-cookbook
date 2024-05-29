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
#include "updi_bitbang.h"

uint32_t t1coeff;
uint32_t pinmask, pinmaskpower;
uint8_t retbuff[256];
uint8_t * retbuffptr = 0;

void sandbox_main()
{	
	REG_WRITE( IO_MUX_GPIO6_REG, 1<<FUN_IE_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.  Optional: 10k pull-up resistor.
	REG_WRITE( IO_MUX_GPIO7_REG, 1<<FUN_IE_S | 3<<FUN_DRV_S );  //VCC for part 40mA drive.

// | 1<<FUN_PU_S 
// | 1<<FUN_PU_S |

	retbuffptr = retbuff;

	pinmask = 1<<6;
	pinmaskpower = 1<<7 | 1<<11 | 1<<12;

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

	UPDIPowerOn( pinmask, pinmaskpower );
	int clocks_per_bit = 0;
	uint8_t sib[17] = { 0 };
	int r = UPDISetup( pinmask, m.freq_mhz, 115200, &clocks_per_bit, sib );
	uprintf( "UPDISetup() = %d -> %s\n", r, sib );

	r = UPDIErase( pinmask, clocks_per_bit );
	uprintf( "UPDIErase() = %d\n", r );

	uint8_t  testprog[] = {
0x12,0xc0,0x24,0xc0,0x23,0xc0,0x22,0xc0,0x21,0xc0,0x20,0xc0,0x1f,0xc0,0x1e,0xc0,
0x1d,0xc0,0x1c,0xc0,0x1b,0xc0,0x1a,0xc0,0x19,0xc0,0x18,0xc0,0x17,0xc0,0x16,0xc0,
0x15,0xc0,0x14,0xc0,0x13,0xc0,0x11,0x24,0x1f,0xbe,0xcf,0xe5,0xd1,0xe0,0xde,0xbf,
0xcd,0xbf,0x10,0xe0,0xa0,0xe6,0xb0,0xe0,0xe2,0xea,0xf0,0xe0,0x2,0xc0,0x5,0x90,
0xd,0x92,0xa2,0x36,0xb1,0x7,0xd9,0xf7,0x2,0xd0,0x29,0xc0,0xd9,0xcf,0xe0,0x91,
0x60,0x0,0xf0,0x91,0x61,0x0,0x80,0x81,0x81,0x60,0x80,0x83,0xe0,0x91,0x60,0x0,
0xf0,0x91,0x61,0x0,0x84,0x81,0x81,0x60,0x84,0x83,0x2f,0xef,0x84,0xe3,0x9c,0xe0,
0x21,0x50,0x80,0x40,0x90,0x40,0xe1,0xf7,0x0,0xc0,0x0,0x0,0xe0,0x91,0x60,0x0,
0xf0,0x91,0x61,0x0,0x84,0x81,0x8e,0x7f,0x84,0x83,0x2f,0xef,0x84,0xe3,0x9c,0xe0,
0x21,0x50,0x80,0x40,0x90,0x40,0xe1,0xf7,0x0,0xc0,0x0,0x0,0xdf,0xcf,0xf8,0x94,
0xff,0xcf,0x40,0x4 };

	r = UPDIFlash( pinmask, clocks_per_bit, testprog, sizeof(testprog), 0);
	printf( "UPDIFlash() = %d\n", r );

	uprintf( "...\n" );
}

void teardown()
{
	UPDIPowerOff( pinmask, pinmaskpower );
	// Power-Down
	GPIO.out_w1tc = 1<<6;
	GPIO.out_w1ts = 1<<6;
	GPIO.out_w1tc = 1<<7;
}

void sandbox_tick()
{
	//uprintf( "...\n" );

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
}

int ch32v003_usb_feature_report( uint8_t * buffer, int reqlen, int is_get )
{
#if 0
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
#endif

	return 0;
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnDecom = teardown,
	.fnAdvancedUSB = ch32v003_usb_feature_report
};
