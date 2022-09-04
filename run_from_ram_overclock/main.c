
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

int ram_main()
{
	// To let the monitor attach.
	esp_rom_delay_us( 40000 );
	esp_rom_printf( "Starting\n" );
	esp_rom_delay_us( 1000 ); // Allow enough time for the string to print.

	DPORT_SET_PERI_REG_MASK( DPORT_CPU_PERI_CLK_EN_REG, DPORT_CLK_EN_DEDICATED_GPIO );
	DPORT_CLEAR_PERI_REG_MASK( DPORT_CPU_PERI_RST_EN_REG, DPORT_RST_EN_DEDICATED_GPIO);

	// Setup GPIO16 to be the PRO ALONE output.
	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<16 );
	REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<16 );
	REG_WRITE( IO_MUX_GPIO16_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC16_OUT_SEL_CFG_REG, PRO_ALONEGPIO_OUT0_IDX );
	REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output

	// Disable interrupts.  Period.
	XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);

	// Setup the super watchdog to auto-feed.
	REG_SET_BIT(RTC_CNTL_SWD_CONF_REG, RTC_CNTL_SWD_AUTO_FEED_EN);

	// Seems to have no impact.
    //SET_PERI_REG_MASK(RTC_CNTL_REG, RTC_CNTL_DBOOST_FORCE_PD);

	// Disable the normal watchdog.
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, RTC_CNTL_WDT_WKEY_VALUE);
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG0_REG, 0 );
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, 0);

	// XXX DO NOT DELETE - this prevents the TG0WDT_SYS_RST from hitting. 
	REG_CLR_BIT( DPORT_PERIP_CLK_EN0_REG, DPORT_TIMERGROUP_CLK_EN );
	
	// Change system frequency 

	// This sets the CPU VBIAS, If you go too fast (like >300 MHz on 1.1V)
	// it will cause the CPU to crash.  You can turn this down if you don't need it.
	REG_SET_FIELD(RTC_CNTL_REG, RTC_CNTL_DIG_DBIAS_WAK, RTC_CNTL_DBIAS_1V25);
	REG_SET_FIELD(RTC_CNTL_REG, RTC_CNTL_DIG_DBIAS_SLP, RTC_CNTL_DBIAS_1V25);

	// SYSTEM_SOC_CLK_SEL  (Divisors for CPU (we will actually set later))
	// 0 = XTAL
	// 1 = PLL (default)
	// 2 = CLK8M
	// 3 = APLL_CLK
	int clock_source = 1; //SEL_0
	int SEL_1_PLL = 0; //SEL_1
	int SEL_2_CDIV = 0; //SEL_2  

	// SEL_1 / SEL_2
	//  0		0	  F_PLL/2
	//  1		0	  F_PLL/3
	//  0		1	  F_PLL/1
	//  1		1	  F_PLL/3*2
	//  0		2	  F_PLL/1
	//  1		2	  F_PLL/1
	//  0		3	  F_PLL/1
	//  1		3	  F_PLL/1

	// If CDIV = 0, 480->80MHz
	// If CDIV = 1, 480->160MHz
	// If CDIV = 2, 480->240MHz
	// If CDIV = 3, 480->240MHz
	//   "PLL" seems to have no impact.

	int div_ref = 0;
	int div7_0 = 8;

	// Make sure PLL is not selected.
	CLEAR_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_PLL_FREQ_SEL);

	// Warning the main clock PLL cannot output < 80 MHz safely.  (Most chips seem to be able to go down to 60, but it's risky)
	div_ref = 7; 
	div7_0 = 28;
	// Freq = 20 / (1+div_ref) * ( div7_0 + 4 )

	#define F_PLL FAST  
	int FCAL_MODE = 0;
	int normal_dhref = 1;
	int normal_dlref = 2;
#if (F_PLL == 400)
	div_ref = 7;
	div7_0 = 156;
	SEL_1_PLL = 0;
	SEL_2_CDIV = 3;
#elif (F_PLL == 500)
	div_ref = 2;
	div7_0 = 96;
	SEL_1_PLL = 0;
	SEL_2_CDIV = 2;
	normal_dlref = 0;
#elif (F_PLL == FAST)
	// Just an unattainable frequency.
	div_ref = 4;
	div7_0 = 255;
	SEL_1_PLL = 0;
	SEL_2_CDIV = 2;
	normal_dhref = 2; // Go un-regulated.
	normal_dlref = 0;
	FCAL_MODE = 1;
#elif (F_PLL == 80)
	div_ref = 7;
	div7_0 = 28;
	SEL_1_PLL = 0;
	SEL_2_CDIV = 2;
#endif


	// Bits
	// [7] = Low power (limits headroom) (I2C_BBPLL_OC_TSCHGP)??
	// [6] = Boost (Adds headroom to PLL peak) (I2C_BBPLL_OC_ENB_VCON)
	// [5] = if set *don't* do /2 on PLL output. (I2C_BBPLL_DIV_CPU)
	// [4] = I2C_BBPLL_DIV_DAC (NOTE: THIS HAS AN IMPACT ON OPERATION???) If set, cannot go past ~450 MHz 
	// [3] = I2C_BBPLL_DIV_ADC
	// [2] = I2C_BBPLL_DIV_ADC
	// [1] = I2C_BBPLL_MODE_HF
	// [0] = I2C_BBPLL_RSTB_DIV_ADC_MSB
	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_MODE_HF, 0x6e);   // Address 4.

	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_IR_CAL_DELAY, 0xff);  //Address 0 = 0xff Seems to make it more reliable (in conjunction with Address 2's MSB)

	// address 1.
	// NOTE: setting I2C_BBPLL_IR_CAL_ENX_CAP seems to limit processor speed.  But might? allow total overall clocking (investigate)
	// Note: Must set I2C_BBPLL_IR_CAL_RSTB_MSB otehrwise it will be slower, it seems?
	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_IR_CAL_START, 1<<I2C_BBPLL_IR_CAL_RSTB_MSB ); 

	#define dchgp 0 // as it goes up, peak frequency goes down. (0..7)
	#define dcur 1  // as it goes up, peak frequency goes down.
	#define i2c_bbpll_div_7_0 div7_0

	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_REF_DIV, (dchgp << I2C_BBPLL_OC_DCHGP_LSB) | (div_ref) | FCAL_MODE<<I2C_BBPLL_OC_ENB_FCAL_LSB );    // Address 2  (div_ref is bottom nibble)
	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DIV_7_0, i2c_bbpll_div_7_0); // Address 3

	// NOTE: This register is actually responsible for DR1, DR3 and USB
	// But there is no known impact from DR1 and DR3 bits, they seem to do nothing.
	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DR1, 0x80);  // Address 5

	// normal_dhref: Normally 1, setting to 0 gives us more margin. It seems to relaibly generate 1GHz for the 500 MHz CPU clock.
	REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DCUR, 
		(normal_dlref << I2C_BBPLL_OC_DLREF_SEL_LSB ) | 
		(normal_dhref << I2C_BBPLL_OC_DHREF_SEL_LSB) | 
		dcur
		);  // Address 6

	// Addresses 7+ don't seem to do anything we care about.

	// TRICKY: We switch the CPU over immediately, so that it can sync onto the PLL, otherwise,
	// if we wait (like we should) we could miss the boat.  This is only a concern it seems when going > 400 MHz
	REG_WRITE( DPORT_CPU_PER_CONF_REG, ( DPORT_REG_READ( DPORT_CPU_PER_CONF_REG ) & 0xfffffff8 ) | (SEL_1_PLL << 2) | (SEL_2_CDIV << 0) );
	REG_WRITE( DPORT_SYSCLK_CONF_REG, ( DPORT_REG_READ( DPORT_SYSCLK_CONF_REG ) & 0xfffff3ff ) | (clock_source << 10) );


#define NOP5		__asm__ __volatile__ ("nop\nnop\nnop\nnop\nnop");
#define NOP10  NOP5 NOP5

//	main_loop();

	do
	{
		// Create a nice little pulse train at 1/10th 
		__asm__ __volatile__ ("set_bit_gpio_out 0x1");
		__asm__ __volatile__ ("nop\nnop\nnop\nnop");

//		NOP5 NOP10 NOP10 NOP10 NOP10 
//		NOP10 NOP10 NOP10 NOP10 NOP10 NOP10 NOP10 NOP10 NOP10 NOP10

		__asm__ __volatile__ ("clr_bit_gpio_out 0x1");
		// Loop takes 4 nop's.

//		NOP10 NOP10 NOP10 NOP10 
//		NOP5

	} while(1);

	return 0;
}

