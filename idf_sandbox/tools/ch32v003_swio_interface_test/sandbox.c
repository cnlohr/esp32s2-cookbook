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
uint32_t pinmask;

void sandbox_main()
{
	REG_WRITE( IO_MUX_GPIO6_REG, 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.
	//GPIO5 is wired to pullup.
	//GPIO.out_w1ts = 1<<5;
	//GPIO.enable_w1ts = 1<<5;

	pinmask = 1<<6;
	GPIO.out_w1ts = pinmask;
	GPIO.enable_w1ts = pinmask;

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

//	DoSongAndDanceToEnterPgmMode( t1coeff, pinmask );
	SendWord32( t1coeff, pinmask, 0x7e, 0x5aa50000 | (1<<10) ); // Shadow Config Reg
	SendWord32( t1coeff, pinmask, 0x7d, 0x5aa50000 | (1<<10) ); // CFGR (1<<10 == Allow output from slave)

	uint32_t rval = 0;
	int r = ReadWord32( t1coeff, pinmask, 0x7c, &rval ); // Capability Register (CPBR)
	uprintf( "CPBR: %d - %08x %08x\n", r, rval, REG_READ( GPIO_IN_REG ) );
	
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Make the debug module work properly.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Initiate a halt request.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 1 ); // Clear halt request bit.

	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x40000001 ); // Resume
	
	r = ReadWord32( t1coeff, pinmask, 0x11, &rval ); // 
	uprintf( "DMSTATUS: %d - %08x %08x\n", r, rval, REG_READ( GPIO_IN_REG ) );
}

void sandbox_tick()
{
	int r;
	uint32_t rval;
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Make the debug module work properly.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Initiate a halt request.
	SendWord32( t1coeff, pinmask, CDMCONTROL, 1 ); // Clear halt request bit.
	r = ReadWord32( t1coeff, pinmask, CDATA0, &rval ); // 0x04 = DATA0
//	r = ReadWord32( t1coeff, pinmask, 0x05, &rval ); // 
//	uprintf( "DMSTATUS: %d - %08x\n", r, rval );
	SendWord32( t1coeff, pinmask, CDATA0, 0x12349999 ); // Reset Debug Subsystem
//	uprintf( "DMSTATUS: %d - %08x\n", r, rval );
//	SendWord32( t1coeff, pinmask, 0x05, 0x789a4444 ); // Reset Debug Subsystem
	esp_rom_delay_us(100);
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnAdvancedUSB = NULL
};
