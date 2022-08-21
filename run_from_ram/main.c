
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
#include "esp_rom_sys.h"
#include "xtensa/xtruntime.h"

int ets_printf( const char *restrict format, ... );

uint32_t marker = 0xaabbcc12;
uint8_t markx[16] = { 'h', 'e', 'l', 'l', 'o', 0 };

void uart_tx_flush();
void rom_i2c_writeReg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
void rom_i2c_writeReg_Mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);

void main_loop();

#define regi2c_ctrl_write_reg_mask rom_i2c_writeReg_Mask
#define regi2c_ctrl_write_reg      rom_i2c_writeReg

int ram_main()
{
	esp_rom_delay_us( 50000 );
	esp_rom_printf( "Test Starting BootReason=%d\n", esp_rom_get_reset_reason(0) );
	esp_rom_delay_us( 10000 );
	esp_rom_delay_us( 10000 );
	esp_rom_delay_us( 10000 );
	esp_rom_delay_us( 10000 );
	esp_rom_delay_us( 10000 );
	esp_rom_delay_us( 10000 );

	DPORT_SET_PERI_REG_MASK( DPORT_CPU_PERI_CLK_EN_REG, DPORT_CLK_EN_DEDICATED_GPIO );
    DPORT_CLEAR_PERI_REG_MASK( DPORT_CPU_PERI_RST_EN_REG, DPORT_RST_EN_DEDICATED_GPIO);


	// Setup GPIO16 to be the PRO ALONE output.
	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<16 );
	REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<16 );
	REG_WRITE( IO_MUX_GPIO16_REG, 2<<FUN_DRV_S );
	DPORT_REG_WRITE( GPIO_FUNC16_OUT_SEL_CFG_REG, PRO_ALONEGPIO_OUT0_IDX );

	DPORT_REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output

	// Disable interrupts.  Period.
	XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);

	// This sets the CPU VBIAS, If you go too fast (like >300 MHz on 1.1V)
	// it will cause the CPU to crash.  You can turn this down if you don't need it.
    REG_SET_FIELD(RTC_CNTL_REG, RTC_CNTL_DIG_DBIAS_WAK, RTC_CNTL_DBIAS_1V25);
    REG_SET_FIELD(RTC_CNTL_REG, RTC_CNTL_DIG_DBIAS_SLP, RTC_CNTL_DBIAS_1V25);

	//	We don't want the BBPLL because we aren't doing wifi.
	CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BBPLL_FORCE_PU);
	CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BBPLL_I2C_FORCE_PU);
	CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BB_I2C_FORCE_PU);
	CLEAR_PERI_REG_MASK(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_FORCE_PU);

    SET_PERI_REG_MASK(RTC_CNTL_REG, RTC_CNTL_DBOOST_FORCE_PD);

	// Look into DBOOST

#if 0
	// Bring up RTC.  (Portions from rtc_init.c)
	// Note: There are many things not done here.
	// Like wifi clock bringup, or even external memory support.
	// see rtc_init.c for more info.
    CLEAR_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PVTMON_PU);
    REG_SET_FIELD(RTC_CNTL_TIMER1_REG, RTC_CNTL_PLL_BUF_WAIT, 5);
    REG_SET_FIELD(RTC_CNTL_TIMER1_REG, RTC_CNTL_CK8M_WAIT, 5);

    /* Moved from rtc sleep to rtc init to save sleep function running time */
    // set shortest possible sleep time limit
    REG_SET_FIELD(RTC_CNTL_TIMER5_REG, RTC_CNTL_MIN_SLP_VAL, RTC_CNTL_MIN_SLP_VAL_MIN);

    // set rtc peri timer
    REG_SET_FIELD(RTC_CNTL_TIMER4_REG, RTC_CNTL_POWERUP_TIMER, 1);
    REG_SET_FIELD(RTC_CNTL_TIMER4_REG, RTC_CNTL_WAIT_TIMER, 1);
    // set digital wrap timer
    REG_SET_FIELD(RTC_CNTL_TIMER4_REG, RTC_CNTL_DG_WRAP_POWERUP_TIMER, 1);
    REG_SET_FIELD(RTC_CNTL_TIMER4_REG, RTC_CNTL_DG_WRAP_WAIT_TIMER, 1);
    // set rtc memory timer
    REG_SET_FIELD(RTC_CNTL_TIMER5_REG, RTC_CNTL_RTCMEM_POWERUP_TIMER, 1);
    REG_SET_FIELD(RTC_CNTL_TIMER5_REG, RTC_CNTL_RTCMEM_WAIT_TIMER, 1);

    SET_PERI_REG_MASK(RTC_CNTL_BIAS_CONF_REG, RTC_CNTL_DEC_HEARTBEAT_WIDTH | RTC_CNTL_INC_HEARTBEAT_PERIOD);

    /* Recover default wait cycle for touch or COCPU after wakeup from deep sleep. */
    REG_SET_FIELD(RTC_CNTL_TIMER2_REG, RTC_CNTL_ULPCP_TOUCH_START_WAIT, RTC_CNTL_ULPCP_TOUCH_START_WAIT_DEFAULT);


	if(  1 )
	{
        CLEAR_PERI_REG_MASK(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_FORCE_PU);
        //cancel xtal force pu if no need to force power up
        //cannot cancel xtal force pu if pll is force power on
       // if (!(cfg.xtal_fpu | cfg.bbpll_fpu)) {
       //     CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_XTL_FORCE_PU);
       // } else {
            SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_XTL_FORCE_PU);
       // }
        // CLEAR APLL close
        CLEAR_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU);
        SET_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD);

        //cancel bbpll force pu if setting no force power up
        if (1) {
            CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BBPLL_FORCE_PU);
            CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BBPLL_I2C_FORCE_PU);
            CLEAR_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BB_I2C_FORCE_PU);
        } else {
            SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BBPLL_FORCE_PU);
            SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BBPLL_I2C_FORCE_PU);
            SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_BB_I2C_FORCE_PU);
        }
        //cancel RTC REG force PU
        CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_FORCE_PU);
        CLEAR_PERI_REG_MASK(RTC_CNTL_REG, RTC_CNTL_REGULATOR_FORCE_PU);
        CLEAR_PERI_REG_MASK(RTC_CNTL_REG, RTC_CNTL_DBOOST_FORCE_PU);

        //combine two rtc memory options
        CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_MEM_FORCE_PU);
        CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_MEM_FORCE_NOISO);

        if (1) {
            SET_PERI_REG_MASK(RTC_CNTL_REG, RTC_CNTL_DBOOST_FORCE_PD);
        } else {
            CLEAR_PERI_REG_MASK(RTC_CNTL_REG, RTC_CNTL_DBOOST_FORCE_PD);
        }
        //cancel sar i2c pd force
        CLEAR_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG,RTC_CNTL_SAR_I2C_FORCE_PD);
        //cancel digital pu force
        CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_MEM_FORCE_PU);

        /* If this mask is enabled, all soc memories cannot enter power down mode */
        /* We should control soc memory power down mode from RTC, so we will not touch this register any more */
        CLEAR_PERI_REG_MASK(DPORT_MEM_PD_MASK_REG, DPORT_LSLP_MEM_PD_MASK);
        /* If this pd_cfg is set to 1, all memory won't enter low power mode during light sleep */
        /* If this pd_cfg is set to 0, all memory will enter low power mode during light sleep */
      //  rtc_sleep_pd_config_t pd_cfg = RTC_SLEEP_PD_CONFIG_ALL(0);
      //  rtc_sleep_pd(pd_cfg);

        CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_PWC_REG, RTC_CNTL_DG_WRAP_FORCE_PU);
        CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_PWC_REG, RTC_CNTL_WIFI_FORCE_PU);
        // ROM_RAM power domain is removed
        // CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_PWC_REG, RTC_CNTL_CPU_ROM_RAM_FORCE_PU);
        CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_DG_WRAP_FORCE_NOISO);
        CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_WIFI_FORCE_NOISO);
        // CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_CPU_ROM_RAM_FORCE_NOISO);
        CLEAR_PERI_REG_MASK(RTC_CNTL_PWC_REG, RTC_CNTL_FORCE_NOISO);
        //cancel digital PADS force no iso
        if (1) {
            CLEAR_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_CPU_WAIT_MODE_FORCE_ON);
        } else {
            SET_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_CPU_WAIT_MODE_FORCE_ON);
        }
        /*if DPORT_CPU_WAIT_MODE_FORCE_ON == 0 , the cpu clk will be closed when cpu enter WAITI mode*/

        CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_DG_PAD_FORCE_UNHOLD);
        CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_DG_PAD_FORCE_NOISO);
    }
    /* force power down wifi and bt power domain */
    SET_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_WIFI_FORCE_ISO);
    SET_PERI_REG_MASK(RTC_CNTL_DIG_PWC_REG, RTC_CNTL_WIFI_FORCE_PD);

#endif





	// Change system frequency 

    int div_ref = 0;
    int div7_0 = 8;
    int dr1 = 0;
    int dr3 = 0;
    int dchgp;
    int dcur;


	// Make sure PLL is not selected.
    CLEAR_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_PLL_FREQ_SEL);

    div_ref = 7; 
		// 0: 80 MHz
		// 1: 40 MHz base (+10MHz per div)
		// 2: 26.6666 + 6.6666 per div
		// 3: 20 MHz + 5 per div
		// 4: 16 MHz + 4 per div
    div7_0 = 206;
		// NOTE: Uc @ 80 when @ 480M
		// @ 0x6B, div7_0 = 4 = 53.333 MHz
		// @ 0x6B, div7_0 = 8 = 80 MHz
		// @ 0x6B, div7_0 = 10 = 92.6 MHz
		// @ 0x6B, div7_0 = 11 = 100 MHz
		// @ 0x6B -> Will not work at = 12 (When conencted to bus)

#define RUN_AT_400MHZ
#ifdef RUN_AT_400MHZ
	div_ref = 7;
	div7_0 = 156;
#elif defined( RUN_AT_500MHZ)
	div_ref = 7;
	div7_0 = 206;
#endif


	// Freq = 20 / (1+div_ref) * ( div7_0 + 4 )
	// 500  = 20 / (1+7) * (196+4)

    dr1 = 0; //No apparent impact?
    dr3 = 0; //No apparent impact?
    dchgp = 0; // as it goes up, peak frequency goes down.
    dcur = 1;  // as it goes up, peak frequency goes down.
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_MODE_HF, 0x6e);


    uint8_t i2c_bbpll_lref  = (dchgp << I2C_BBPLL_OC_DCHGP_LSB) | (div_ref);
    uint8_t i2c_bbpll_div_7_0 = div7_0;
    uint8_t i2c_bbpll_dcur = (2 << I2C_BBPLL_OC_DLREF_SEL_LSB ) | (1 << I2C_BBPLL_OC_DHREF_SEL_LSB) | dcur;
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_REF_DIV, i2c_bbpll_lref);
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DIV_7_0, i2c_bbpll_div_7_0);
    REGI2C_WRITE_MASK(I2C_BBPLL, I2C_BBPLL_OC_DR1, dr1);
    REGI2C_WRITE_MASK(I2C_BBPLL, I2C_BBPLL_OC_DR3, dr3);
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DCUR, i2c_bbpll_dcur);


	// SYSTEM_SOC_CLK_SEL
	// 0 = XTAL
	// 1 = PLL (default)
	// 2 = CLK8M
	// 3 = APLL_CLK
	const int clock_source = 1; //SEL_0
	const int SEL_1_PLL = 0; //SEL_1
	const int SEL_2_CDIV = 3; //SEL_2  

	// If CDIV = 0, 480->80MHz
	// If CDIV = 1, 480->160MHz
	// If CDIV = 2, 480->240MHz
	// If CDIV = 3, 480->240MHz
	//   "PLL" seems to have no impact.

	DPORT_REG_WRITE( DPORT_CPU_PER_CONF_REG, ( DPORT_REG_READ( DPORT_CPU_PER_CONF_REG ) & 0xfffffff8 ) | (SEL_1_PLL << 2) | (SEL_2_CDIV << 0) ); // SYSTEM_PLL_FREQ_SEL, SYSTEM_CPUPERIOD_SEL
	DPORT_REG_WRITE( DPORT_SYSCLK_CONF_REG, ( DPORT_REG_READ( DPORT_SYSCLK_CONF_REG ) & 0xfffff3ff ) | (clock_source << 10) );


#define NOP5		__asm__ __volatile__ ("nop\nnop\nnop\nnop\nnop");
#define NOP10  NOP5 NOP5


	// Lastly we disable the WDT.  WHY IS THIS BROKEN?
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, RTC_CNTL_WDT_WKEY_VALUE);
	REG_SET_BIT(RTC_CNTL_WDTFEED_REG, RTC_CNTL_WDT_FEED);
	REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_STG0, RTC_WDT_STAGE_ACTION_OFF);
	REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_STG1, RTC_WDT_STAGE_ACTION_OFF);
	REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_STG2, RTC_WDT_STAGE_ACTION_OFF);
	REG_SET_FIELD(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_STG3, RTC_WDT_STAGE_ACTION_OFF);
	REG_CLR_BIT(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_FLASHBOOT_MOD_EN);
	REG_CLR_BIT(RTC_CNTL_WDTCONFIG0_REG, RTC_CNTL_WDT_EN);
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG1_REG, 0xffffffff );
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG2_REG, 0xffffffff );
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG3_REG, 0xffffffff );
	WRITE_PERI_REG( RTC_CNTL_WDTCONFIG4_REG, 0xffffffff );
	WRITE_PERI_REG(RTC_CNTL_WDTWPROTECT_REG, 0);

	main_loop();
/*
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
*/
	return 0;
}

