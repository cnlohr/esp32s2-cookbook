
char temp[1024];

#include "soc/dport_access.h"
#include "soc/rtc_wdt.h"
#include "soc/system_reg.h"
#include "soc/soc.h"  // for WRITE_PERI_REG
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc.h"
#include "soc/gpio_sig_map.h"
#include "soc/dedic_gpio_reg.h"
#include "regi2c_ctrl.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_rom_sys.h"
#include "xtensa/xtruntime.h"

int ets_printf( const char *restrict format, ... );

void uart_tx_flush();
void rom_i2c_writeReg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
void rom_i2c_writeReg_Mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);

void main_loop();

#define regi2c_ctrl_write_reg_mask rom_i2c_writeReg_Mask
#define regi2c_ctrl_write_reg	  rom_i2c_writeReg

void uart_tx_one_char( char c );

int hueval( int i )
{
	i = i & 511;
	if( i < 85 )
	{
		return i*3+1;
	}
	else if( i < 256 )
	{
		return 255;
	}
	else if( i < 341 )
	{
		return 1022 - i * 3;
	}
	return 0;
}

int ram_main()
{
	// To let the monitor attach.
	esp_rom_printf( "Starting\n" );

	DPORT_SET_PERI_REG_MASK( DPORT_CPU_PERI_CLK_EN_REG, DPORT_CLK_EN_DEDICATED_GPIO );
	DPORT_CLEAR_PERI_REG_MASK( DPORT_CPU_PERI_RST_EN_REG, DPORT_RST_EN_DEDICATED_GPIO);

	// Setup GPIO18 to be the PRO ALONE output.
	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<18 );
	REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<18 );
	REG_WRITE( IO_MUX_GPIO18_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC18_OUT_SEL_CFG_REG, PRO_ALONEGPIO_OUT0_IDX );
	REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output


	// Disable interrupts.  Period.
	XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);

	// Setup the super watchdog to auto-feed.
	REG_SET_BIT(RTC_CNTL_SWD_CONF_REG, RTC_CNTL_SWD_AUTO_FEED_EN);

	// Disable the normal watchdog.
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, RTC_CNTL_WDT_WKEY_VALUE);
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG0_REG, 0 );
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, 0);

	// XXX DO NOT DELETE - this prevents the TG0WDT_SYS_RST from hitting. 
	REG_CLR_BIT( DPORT_PERIP_CLK_EN0_REG, DPORT_TIMERGROUP_CLK_EN );

	#define nop __asm__ __volatile__("nop\n")

	int frame = 0;

	do
	{
		int spin;

		// Create an RGB Rainbow value
		uint32_t ws_out = hueval( frame ) | ( hueval( frame + 170 ) << 8 ) | ( hueval( frame + 341 ) << 16 );

		int i;
		// Shift out all 24 bits, note the 1 and 3 for timing is baesed off an XTAL clock.
		for( i = 0; i < 24; i++ )
		{
			__asm__ __volatile__ ("set_bit_gpio_out 0x1");

			int high_for = ( ws_out & 0x800000 )?3:1;
			for( spin = 0; spin < high_for; spin++ ) nop;

			__asm__ __volatile__ ("clr_bit_gpio_out 0x1");
			int low_for = ( ws_out & 0x800000 )?1:3;
			for( spin = 0; spin < low_for; spin++ ) nop;

			ws_out<<=1;
		}

		// Wait 5ms.
		esp_rom_delay_us( 5000 );
		frame++;
	} while( 1 );

	return 0;
}

