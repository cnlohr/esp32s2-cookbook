
char temp[1024];

#include "soc/dport_access.h"
#include "rtc_wdt.h"
#include "soc/system_reg.h"
#include "soc/soc.h"  // for WRITE_PERI_REG
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc.h"
#include "soc/gpio_sig_map.h"
//#include "soc/dedic_gpio_reg.h"
#include "regi2c_ctrl.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_rom_sys.h"
#include "xtensa/xtruntime.h"

// For LEDC
#include "soc/ledc_reg.h"

// For I2S
#include "soc/i2s_reg.h"

int ets_printf( const char *restrict format, ... );

void uart_tx_flush();
void rom_i2c_writeReg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
void rom_i2c_writeReg_Mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);

void main_loop();

#define regi2c_ctrl_write_reg_mask rom_i2c_writeReg_Mask
#define regi2c_ctrl_write_reg	  rom_i2c_writeReg

void uart_tx_one_char( char c );


void DoI2SStuff()
{
	//periph_module_enable(PERIPH_I2S0_MODULE);

	DPORT_SET_PERI_REG_MASK( SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_I2S0_CLK_EN );
	DPORT_CLEAR_PERI_REG_MASK( SYSTEM_PERIP_RST_EN0_REG, SYSTEM_I2S0_RST);

	//WRITE_PERI_REG( I2S_LC_CONF_REG(0), 1<<I2S_IN_RST_S | 1<<I2S_AHBM_RST_S | 1<<I2S_AHBM_FIFO_RST_S );
	//WRITE_PERI_REG( I2S_LC_CONF_REG(0), 0);
	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), 0x1, 1, I2S_TX_RESET_S);
	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), 0x1, 0, I2S_TX_RESET_S);
	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), 0x1, 1, I2S_TX_FIFO_RESET_S);
	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), 0x1, 0, I2S_TX_FIFO_RESET_S);

	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), 0x1, 0, I2S_TX_SLAVE_MOD_S);  //Needed, otherwise it waits for a clock.

	// Configures APLL = 480 / 4 = 120
	// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );
	// Datasheet recommends that numerator does not exceed 500MHz.
	//local_rtc_clk_apll_enable( 1, 0, 0, 8, 0 );

	// According to TRM I2S_TX_BCK_DIV_NUM must not be set to 1.  But that's the only way to get this I2S to do anything fun.
	// In case you want to go fast, be sure to set it to 1!  Then you can output at up to 80MHz with the 160M clock.
	WRITE_PERI_REG(I2S_TX_CONF1_REG(0), (32<<I2S_TX_BITS_MOD_S ) | ( 2<<I2S_TX_BCK_DIV_NUM_S)  );
		// | (32<<I2S_RX_BITS_MOD_S) | (2<<I2S_RX_BCK_DIV_NUM_S) );  

	// fI2S = fCLK / ( N + B/A )
	// For 250kBAUD, N=40 (because of other /2's) if selecting 160M clock.
	// DIV_NUM = N
	// Note I2S_CLKM_DIV_NUM minimum = 2.
	WRITE_PERI_REG( I2S_TX_CLKM_CONF_REG(0), (2<<I2S_TX_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_RX_CLKM_DIV_NUM_S) | I2S_TX_CLK_ACTIVE );  // Minimum reduction, 2:1

///	WRITE_PERI_REG( I2S_FIFO_CONF_REG(0), (32<<I2S_RX_DATA_NUM_S) | (32<<I2S_TX_DATA_NUM_S) );
	WRITE_PERI_REG( I2S_TX_CONF_REG(0), (1<<I2S_TX_CHAN_MOD_S) );

	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), I2S_TX_STOP_EN_V, 1, I2S_TX_STOP_EN_S);
	SET_PERI_REG_BITS(I2S_TX_CONF_REG(0), I2S_TX_PCM_BYPASS_V, 1, I2S_TX_PCM_BYPASS_S);

//	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_TX_RIGHT_FIRST_V, 1, I2S_TX_RIGHT_FIRST_S);
//	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_RX_RIGHT_FIRST_V, 1, I2S_RX_RIGHT_FIRST_S);

	WRITE_PERI_REG( I2S_TX_TIMING_REG(0), 0 );

	// Start returning data to the application
//	esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, i2s_isr_fast, (void*)REG_I2S_BASE(0), &i2s_intr_handle);
	esp_intr_enable(i2s_intr_handle);

	REG_WRITE( GPIO_FUNC36_OUT_SEL_CFG_REG, I2S0O_WS_OUT_IDX );
	REG_WRITE( GPIO_FUNC37_OUT_SEL_CFG_REG, I2S0O_DATA_OUT23_IDX );
}



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

	DPORT_SET_PERI_REG_MASK( SYSTEM_CPU_PERI_CLK_EN_REG, SYSTEM_CLK_EN_DEDICATED_GPIO );
	DPORT_CLEAR_PERI_REG_MASK( SYSTEM_CPU_PERI_RST_EN_REG, SYSTEM_RST_EN_DEDICATED_GPIO);

	// Setup GPIO38+37 to be outputs.
	REG_WRITE( GPIO_OUT1_W1TS_REG,    (1<<6)|(1<<5)|(1<<4) );
	REG_WRITE( GPIO_ENABLE1_W1TS_REG, (1<<6)|(1<<5)|(1<<4) ); // GPIO 38 & 37 && 36



	//GPIO38 = PRO ALONE (DEDIC) output.
	REG_WRITE( IO_MUX_GPIO38_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC38_OUT_SEL_CFG_REG, PRO_ALONEGPIO_OUT0_IDX );
//	REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output  (not needed on S3?)

	// Disable interrupts.  Period.
	XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);

	// Setup the super watchdog to auto-feed.
	REG_SET_BIT(RTC_CNTL_SWD_CONF_REG, RTC_CNTL_SWD_AUTO_FEED_EN);

	// Disable the normal watchdog.
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, /*RTC_CNTL_WDT_WKEY_VALUE*/ 0x50D83AA1);
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG0_REG, 0 );
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, 0);

	// XXX DO NOT DELETE - this prevents the TG0WDT_SYS_RST from hitting. 
	REG_CLR_BIT( SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_TIMERGROUP_CLK_EN );

	// SYSTEM_SYSCLK_CONF_REG defaults to: 0x000a8001
	// SYSTEM_CLK_XTAL_FREQ = 168MHz???
	// SYSTEM_SOC_CLK_SEL = 0;
	//  Options are: XTAL_CLK, PLL_CLK, RC_FAST_CLK

	// Switch to PLL, XTAL=40MHz, Pre-Divisor = 1. (SYSTEM_SOC_CLK_SEL = 1)
	WRITE_PERI_REG( SYSTEM_SYSCLK_CONF_REG, 0x00028001 | (SOC_CPU_CLK_SRC_PLL<<10) );

	// SYSTEM_PLL_FREQ_SEL = 1 (For 480MHz PLL)
	// SYSTEM_CPU_WAIT_MODE_FORCE_ON = 1
	// SYSTEM_PLL_FREQ_SEL = 1
	// SYSTEM_CPUPERIOD_SEL = 1 (For 240MHz CPU)
	WRITE_PERI_REG( SYSTEM_CPU_PER_CONF_REG, 0xc | 1 ); 

#if 0
	// Configure LEDC.
	// LEDC = 36,37

	DPORT_SET_PERI_REG_MASK( SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_LEDC_CLK_EN );
	DPORT_CLEAR_PERI_REG_MASK( SYSTEM_PERIP_RST_EN0_REG, SYSTEM_LEDC_RST);

	// LEDC_APB_CLK_SEL = APB_CLK (1)
	// LEDC_CLK_DIV_TIMERx = (0)
	// LEDC_TIMER_x_PAUSE = 0

	REG_WRITE( LEDC_CONF_REG, 1 );

	// Trigger Para Up, unreset.
	REG_WRITE( LEDC_LSTIMER0_CONF_REG, 0x0200001e );

	REG_WRITE( LEDC_LSCH0_HPOINT_REG, 0x3ff );
	REG_WRITE( LEDC_LSCH1_HPOINT_REG, 0x3ff );
	REG_WRITE( LEDC_LSCH0_DUTY_REG, 0x100 );
	REG_WRITE( LEDC_LSCH1_DUTY_REG, 0x100 );

	REG_WRITE( LEDC_LSCH0_CONF0_REG, LEDC_SIG_OUT_EN_LSCH0 | LEDC_PARA_UP_LSCH0 | (0xf<<LEDC_OVF_NUM_LSCH0_S) ); // LEDC_CHn_CONF0_REG
	REG_WRITE( LEDC_LSCH1_CONF0_REG, LEDC_SIG_OUT_EN_LSCH1 | LEDC_PARA_UP_LSCH1 | (0xf<<LEDC_OVF_NUM_LSCH0_S) );

	REG_WRITE( IO_MUX_GPIO36_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC36_OUT_SEL_CFG_REG, LEDC_LS_SIG_OUT0_IDX );
	REG_WRITE( IO_MUX_GPIO37_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC37_OUT_SEL_CFG_REG, LEDC_LS_SIG_OUT1_IDX );
#endif

	// I2S IT IS!
	DoI2SStuff();


	#define nop __asm__ __volatile__("nop\n")

	int frame = 0;

	do
	{
		int spin;
		esp_rom_printf( "%08x\n", (int) REG_READ(LEDC_LSTIMER0_VALUE_REG) );

		// Create an RGB Rainbow value
		uint32_t ws_out = hueval( frame ) | ( hueval( frame + 170 ) << 8 ) | ( hueval( frame + 341 ) << 16 );

		int i;
		// Shift out all 24 bits, note the 1 and 3 for timing is baesed off an XTAL clock.
		for( i = 0; i < 24; i++ )
		{
			__asm__ __volatile__ ("EE.SET_BIT_GPIO_OUT 0x1");

			int high_for = ( ws_out & 0x800000 )?16:7;
			for( spin = 0; spin < high_for; spin++ ) nop;

			__asm__ __volatile__ ("EE.CLR_BIT_GPIO_OUT 0x1");
			int low_for = ( ws_out & 0x800000 )?7:16;
			for( spin = 0; spin < low_for; spin++ ) nop;

			ws_out<<=1;
		}

		// Wait 5ms.
		esp_rom_delay_us( 5000 );
		frame++;
	} while( 1 );

	return 0;
}

