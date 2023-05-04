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

// For clock output
#include "esp_system.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/i2s_reg.h"
#include "soc/periph_defs.h"
#include "rom/lldesc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "soc/regi2c_apll.h"
#include "hal/regi2c_ctrl_ll.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/regi2c_ctrl.h"
#include "hal/clk_tree_ll.h"
#include "pindefs.h"

#define DisableISR()            do { XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL); portbenchmarkINTERRUPT_DISABLE(); } while (0)
#define EnableISR()             do { portbenchmarkINTERRUPT_RESTORE(0); XTOS_SET_INTLEVEL(0); } while (0)

#define MAX_IN_TIMEOUT 1000
#include "ch32v003_swio.h"


uint32_t pinmaskpower;
uint32_t clockpin;
uint8_t retbuff[256];
uint8_t * retbuffptr = 0;
int retisready = 0;

struct SWIOState state;

void sandbox_main()
{
	REG_WRITE( IO_MUX_REG(SWIO_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.  Optional: 10k pull-up resistor. This is the actual SWIO.
	REG_WRITE( IO_MUX_REG(VDD3V3_EN_PIN), 1<<FUN_IE_S | 1<<FUN_PD_S | 3<<FUN_DRV_S );  //VCC for part 40mA drive.
	REG_WRITE( IO_MUX_REG(VDD5V_EN_PIN), 1<<FUN_IE_S | 1<<FUN_PD_S | 3<<FUN_DRV_S );  //5V for part 40mA drive.

	REG_WRITE( IO_MUX_REG(SWIO_PU_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //SWPUC
	GPIO.out_w1ts = 1<<SWIO_PU_PIN;
	GPIO.enable_w1ts = 1<<SWIO_PU_PIN;

	retbuffptr = retbuff;


	memset( &state, 0, sizeof( state ) );
	state.pinmask = 1<<SWIO_PIN;
	pinmaskpower = (1<<VDD3V3_EN_PIN) | (1<<VDD5V_EN_PIN);
	GPIO.out_w1ts = pinmaskpower;
	GPIO.enable_w1ts = pinmaskpower;
	GPIO.out_w1ts = state.pinmask;
	GPIO.enable_w1ts = state.pinmask;

	esp_rom_delay_us(5000);

	rtc_cpu_freq_config_t m;
	rtc_clk_cpu_freq_get_config( &m );
	switch( m.freq_mhz )
	{
	case 240:
		state.t1coeff = 10; // 9 or 10 is good.  5 is too low. 13 is sometimes too high.
		break;
	default:
		state.t1coeff = 100; // Untested At Other Speeds
		break;
	}

#if 0
//	DoSongAndDanceToEnterPgmMode( t1coeff, pinmask );
	WriteReg32( t1coeff, pinmask, 0x7e, 0x5aa50000 | (1<<10) ); // Shadow Config Reg
	WriteReg32( t1coeff, pinmask, 0x7d, 0x5aa50000 | (1<<10) ); // CFGR (1<<10 == Allow output from slave)

	uint32_t rval = 0;
	int r = ReadReg32( t1coeff, pinmask, 0x7c, &rval ); // Capability Register (CPBR)
	uprintf( "CPBR: %d - %08x %08x\n", r, rval, REG_READ( GPIO_IN_REG ) );
	
	#if 0
	WriteReg32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Make the debug module work properly.
	WriteReg32( t1coeff, pinmask, CDMCONTROL, 0x80000001 ); // Initiate a halt request.
	WriteReg32( t1coeff, pinmask, CDMCONTROL, 1 ); // Clear halt request bit.
	WriteReg32( t1coeff, pinmask, CDMCONTROL, 0x40000001 ); // Resume
	#endif
	
	r = ReadReg32( t1coeff, pinmask, 0x11, &rval ); // 
	uprintf( "DMSTATUS: %d - %08x %08x\n", r, rval, REG_READ( GPIO_IN_REG ) );
#endif
}

void teardown()
{
	// Power-Down
	GPIO.out_w1tc = 1<<SWIO_PIN;
	GPIO.out_w1ts = 1<<SWIO_PIN;
	GPIO.out_w1tc = 1<<VDD3V3_EN_PIN;
	GPIO.out_w1tc = 1<<VDD5V_EN_PIN;	
	GPIO.out_w1tc = 1<<SWIO_PU_PIN;
}

void sandbox_tick()
{
	//esp_rom_delay_us(100);
}

// Configures APLL = 480 / 4 = 120
// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );
// Datasheet recommends that numerator does not exceed 500MHz.
void local_rtc_clk_apll_enable(bool enable, uint32_t sdm0, uint32_t sdm1, uint32_t sdm2, uint32_t o_div)
{
	REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD, enable ? 0 : 1);
	REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU, enable ? 1 : 0);

	if (enable) {
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, sdm2);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, sdm0);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, sdm1);
		REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, CLK_LL_APLL_SDM_STOP_VAL_1);
		REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, CLK_LL_APLL_SDM_STOP_VAL_2_REV1);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_OR_OUTPUT_DIV, o_div);
	}
}

int ch32v003_usb_feature_report( uint8_t * buffer, int reqlen, int is_get )
{
	if( is_get )
	{
		if( !retisready ) { buffer[0] = 0xff; return reqlen; }
		retisready = 0;
		int len = retbuffptr - retbuff;
		buffer[0] = len;
		if( len > reqlen-1 ) len = reqlen-1;
		memcpy( buffer+1, retbuff, len );
		retbuffptr = retbuff;
		return reqlen;
	}
	
	// Is send.
	// buffer[0] is the request ID.
	uint8_t * iptr = &buffer[1];
	while( iptr - buffer < reqlen )	
	{
		uint8_t cmd = *(iptr++);
		int remain = reqlen - (iptr - buffer);
		// Make sure there is plenty of space.
		if( (sizeof(retbuff)-(retbuffptr - retbuff)) < 6 ) break;
		if( cmd == 0xfe ) // We will never write to 0x7f.
		{
			cmd = *(iptr++);
			switch( cmd )
			{
			case 0x01:
				DoSongAndDanceToEnterPgmMode( &state );
				break;
			case 0x02: // Power-down 
				uprintf( "Power down\n" );
				// Make sure clock is disabled.
				gpio_matrix_out( GPIO_NUM(MULTI2_PIN), 254, 1, 0 );
				GPIO.out_w1tc = state.pinmask;
				GPIO.enable_w1ts = state.pinmask;
				GPIO.enable_w1tc = pinmaskpower;
				GPIO.out_w1tc = pinmaskpower;
				break;
			case 0x03: // Power-up
				GPIO.out_w1ts = pinmaskpower;
				GPIO.enable_w1ts = pinmaskpower;
				GPIO.enable_w1ts = state.pinmask;
				GPIO.out_w1ts = state.pinmask;
				gpio_matrix_out( GPIO_NUM(SWCLK_PIN), CLK_I2S_MUX_IDX, 1, 0 );
				break;
			case 0x04: // Delay( uint16_t us )
				esp_rom_delay_us(iptr[0] | (iptr[1]<<8) );
				iptr += 2;
				break;
			case 0x05: // Void High Level State
				ResetInternalProgrammingState( &state );
				break;
			case 0x06: // Wait-for-flash-op.
				*(retbuffptr++) = WaitForFlash( &state );
				break;
			case 0x07: // Wait-for-done-op.
				*(retbuffptr++) = WaitForDoneOp( &state );
				break;
			case 0x08: // Write Data32.
			{
				if( remain >= 9 )
				{
					int r = WriteWord( &state, iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24),  iptr[4] | (iptr[5]<<8) | (iptr[6]<<16) | (iptr[7]<<24) );
					iptr += 8;
					*(retbuffptr++) = r;
				}
				break;
			}
			case 0x09: // Read Data32.
			{
				if( remain >= 5 )
				{
					int r = ReadWord( &state, iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24), (uint32_t*)&retbuffptr[1] );
					iptr += 4;
					retbuffptr[0] = r;
					if( r < 0 )
						*((uint32_t*)&retbuffptr[1]) = 0;
					retbuffptr += 5;
				}
				break;
			}
			case 0x0a:
				ResetInternalProgrammingState( &state );
				break;
			case 0x0b:
				if( remain >= 68 )
				{
					int r = Write64Block( &state, iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24), (uint8_t*)&iptr[4] );
					iptr += 68;
					*(retbuffptr++) = r;
				}
				break;
			case 0x0c:
				if( remain >= 8 )
				{
					// Output clock on P2.

					// Maximize the drive strength.
					gpio_set_drive_capability( GPIO_NUM(MULTI2_PIN), GPIO_DRIVE_CAP_2 );

					// Use the IO matrix to create the inverse of TX on pin 17.
					gpio_matrix_out( GPIO_NUM(MULTI2_PIN), CLK_I2S_MUX_IDX, 1, 0 );

					periph_module_enable(PERIPH_I2S0_MODULE);

					int use_apll = *(iptr++);  // try 1
					int sdm0 = *(iptr++);      // try 0
					int sdm1 = *(iptr++);      // try 0
					int sdm2 = *(iptr++);      // try 8
					int odiv = *(iptr++);      // try 0
					iptr +=3 ; // reserved.

					local_rtc_clk_apll_enable( use_apll, sdm0, sdm1, sdm2, odiv );

					if( use_apll )
					{
						WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (1<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (2<<I2S_CLKM_DIV_NUM_S) );
					}
					else
					{
						// fI2S = fCLK / ( N + B/A )
						// DIV_NUM = N
						// Note I2S_CLKM_DIV_NUM minimum = 2 by datasheet.  Less than that and it will ignoreeee you.
						WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (2<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (1<<I2S_CLKM_DIV_NUM_S) );  // Minimum reduction, 2:1
					}
				}
				break;
			case 0x0d:  // Do a terminal log through.
			{
				int tries = 100;
				if( remain >= 8 )
				{
					int r;
					uint32_t leavevalA = iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24);
					iptr += 4;
					uint32_t leavevalB = iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24);
					iptr += 4;
					uint8_t * origretbuf = (retbuffptr++);
					int canrx = (sizeof(retbuff)-(retbuffptr - retbuff)) - 8;
					while( canrx > 8 )
					{
						r = PollTerminal( &state, retbuffptr, canrx, leavevalA, leavevalB );
						origretbuf[0] = r;
						if( r >= 0 )
						{
							retbuffptr += r;
							if( tries-- <= 0 ) break; // ran out of time?
						}
						else
						{
							break;
						}
						canrx = (sizeof(retbuff)-(retbuffptr - retbuff)) -8;
						// Otherwise all is well.  If we aren't signaling try to poll for more data.
						if( leavevalA != 0 || leavevalB != 0 ) break;
					}
				}
				break;
			}
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
					MCFWriteReg32( &state, cmd>>1, iptr[0] | (iptr[1]<<8) | (iptr[2]<<16) | (iptr[3]<<24) );
					iptr += 4;
				}
			}
			else
			{
				if( remain >= 1 && (sizeof(retbuff)-(retbuffptr - retbuff)) >= 4 )
				{
					int r = MCFReadReg32( &state, cmd>>1, (uint32_t*)&retbuffptr[1] );
					retbuffptr[0] = r;
					if( r < 0 )
						*((uint32_t*)&retbuffptr[1]) = 0;
					retbuffptr += 5;
				}
			}
		}
	}

	retisready = 1;

	return 0;
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnDecom = teardown,
	.fnAdvancedUSB = ch32v003_usb_feature_report
};

