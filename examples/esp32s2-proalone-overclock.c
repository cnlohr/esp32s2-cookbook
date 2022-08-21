/*
    Sandbox example of using the ESP32-S2's dedicated GPIO that talks directly to the CPU.
	Additionally, this uses the internal interfaces to try various CPU clocking techniques.
	This also has some primitive assembly.
*/
#include <stdio.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "soc/system_reg.h"
#include "soc/dport_access.h"
#include "soc/dedic_gpio_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_log.h"
#include "soc/rtc.h"
#include "esp32s2/regi2c_ctrl.h"

int global_i = 100;

esp_err_t esp_flash_read(void *chip, void *buffer, uint32_t address, uint32_t length);

void local_rtc_clk_apll_enable(bool enable, uint32_t sdm0, uint32_t sdm1, uint32_t sdm2, uint32_t o_div)
{
    REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD, enable ? 0 : 1);
    REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU, enable ? 1 : 0);

    if (enable) {
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, sdm2);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, sdm0);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, sdm1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_2_REV1);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_OR_OUTPUT_DIV, o_div);
#if 0

        /* calibration  (Doesn't seem to be needed for CPU-only operation) */
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_2);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_3);
        /* wait for calibration end */
        while (!(REGI2C_READ_MASK(I2C_APLL, I2C_APLL_OR_CAL_END))) {
            /* use esp_rom_delay_us so the RTC bus doesn't get flooded */
            esp_rom_delay_us(1);
        }
#endif
    }
}




void sandbox_main(display_t * disp)
{
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );

	DPORT_SET_PERI_REG_MASK( DPORT_CPU_PERI_CLK_EN_REG, DPORT_CLK_EN_DEDICATED_GPIO );
    DPORT_CLEAR_PERI_REG_MASK( DPORT_CPU_PERI_RST_EN_REG, DPORT_RST_EN_DEDICATED_GPIO);
#if 1
	// Somehow, gpio_config in this context does something to prevent us from permacrashing.
    gpio_config_t io_conf={
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1ULL<<GPIO_NUM_16)
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
#else
	// Really, all you need is to configure GPIO_PIN_MUX_REG, and GPIO_ENABLE_W1TS_REG.
	// But I tried finding out why if I do this, it definitely works, but for some reason
	// When overclocking, it can't recover from the crash this way.
	#define gpio_ll_iomux_func_sel(pin_name, func) PIN_FUNC_SELECT( pin_name, func )
	#define gpio_hal_iomux_func_sel(pin_name, func) gpio_ll_iomux_func_sel(pin_name, func)
	esp_err_t rtc_gpio_deinit(gpio_num_t gpio_num);
	void esp_rom_gpio_connect_out_signal(uint32_t gpio_num, uint32_t signal_idx, bool out_inv, bool oen_inv);
	rtc_gpio_deinit( 16 );
	gpio_ll_iomux_func_sel(GPIO_PIN_MUX_REG[16], PIN_FUNC_GPIO);
	DPORT_REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output
	DPORT_REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<GPIO_NUM_16 );
	DPORT_REG_WRITE( GPIO_PIN_MUX_REG[16], 0 ); // Configure GPIO (disable IE, PUs, etc.)
	gpio_set_intr_type(16,0 );
	gpio_intr_disable( 16 );
    esp_rom_gpio_connect_out_signal(16, SIG_GPIO_OUT_IDX, false, false);
#endif

//	gpio_matrix_out( GPIO_NUM_16, PRO_ALONEGPIO_OUT0_IDX, 0, 0 );
	// Don't use the big function if we don't need to.
	DPORT_REG_WRITE( GPIO_FUNC16_OUT_SEL_CFG_REG, PRO_ALONEGPIO_OUT0_IDX ); // Enable CPU instruction output


	DPORT_REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output


	// Disable interrupts.  Period.
	portDISABLE_INTERRUPTS();


	// APLL Appears to max out at sdm2=10, odiv=sdm0=sdm1=0
	// You can push it higher but it doesn't actually hit 160MHz out.
	// So the APLL is much less interesting.  Time to try messing with the real PLL.
	//local_rtc_clk_apll_enable(1, 0 /*sdm0*/, 0 /*sdm1*/, 25 /*sdm2*/, 255 /*odiv*/);
	// Set clock_source = 3, PLL = 0, CDIV = 3.


#if 1

    int div_ref = 0;
    int div7_0 = 8;
    int dr1 = 0;
    int dr3 = 0;
    int dchgp = 5;
    int dcur = 4;

	if( 0 )
	{
		// 480MHz
        /* Clear this register to let the digital part know 480M PLL is used */
        SET_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_PLL_FREQ_SEL);
        /* Configure 480M PLL */
        div_ref = 0;
        div7_0 = 8;
        dr1 = 0;
        dr3 = 0;
        dchgp = 5;
        dcur = 4;
        REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_MODE_HF, 0x6B);
	} else if( 0 ) 
	{
		// 320 MHz
        CLEAR_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_PLL_FREQ_SEL);
        /* Configure 320M PLL */
        div_ref = 0;
        div7_0 = 4;
        dr1 = 0;
        dr3 = 0;
        dchgp = 5;
        dcur = 5;
        REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_MODE_HF, 0x69);
	}
	else
	{
        CLEAR_PERI_REG_MASK(DPORT_CPU_PER_CONF_REG, DPORT_PLL_FREQ_SEL);
        div_ref = 9; 

			// NOTE: This is _I think_ 2X the main CLK frequency.  Just beware these are bonkers high frequencies.
			// 0: 80 MHz
			// 1: 40 MHz base (+10MHz per div)
			// 2: 26.6666 + 6.6666 per div
			// 3: 20 MHz + 5 per div
			// 4: 16 MHz + 4 per div
        div7_0 = 250;
			// NOTE: Uc @ 80 when @ 480M
			// @ 0x6B, div7_0 = 4 = 53.333 MHz
			// @ 0x6B, div7_0 = 8 = 80 MHz
			// @ 0x6B, div7_0 = 10 = 92.6 MHz
			// @ 0x6B, div7_0 = 11 = 100 MHz
			// @ 0x6B -> Will not work at = 12 (When conencted to bus)
/*
        dr1 = 0;
        dr3 = 3;
        dchgp = 0;
        dcur = 5; 
    */    

			// @ 0x6E div_ref = 0 div7_0 = 0 = 80 MHz (when in 3x mode)
			// @ 0x6E div_ref = 0 div7_0 = 1 = 100 MHz (when in 3x mode)
			// @ 0x6E div_ref = 0 div7_0 = 2 = 120 MHz (when in 3x mode)
			// @ 0x6E div_ref = 0 div7_0 = 3 = 140 MHz (when in 3x mode)
			// @ 0x6E div_ref = 0 div7_0 = 9 = 260 MHz (when in 3x mode)
			// @ 0x6E div_ref = 0 div7_0 = 10 = 280 MHz (when in 3x mode)
			// @ 0x6E div_ref = 0 div7_0 = 19 = 460 MHz (when in 3x mode)


			// @ 0x6E div_ref = 1 div7_0 = 19 = 230 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 36 = 400 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 40 = 440 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 44 = 460 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 50 = 460 MHz (when in 3x mode)

/// CHANGE DHCGP = 0
			// @ 0x6E div_ref = 1 div7_0 = 50 = 485 MHz (when in 3x mode)
/// CHANGE DHCGP = 0 & DCUR=7
			// @ 0x6E div_ref = 1 div7_0 = 50 = 480 MHz (when in 3x mode)
/// CHANGE DHCGP = 0 & DCUR=0
			// @ 0x6E div_ref = 1 div7_0 = 50 = 463 MHz (when in 3x mode)
/// CHANGE DHCGP = 0 & DCUR=4
			// @ 0x6E div_ref = 1 div7_0 = 44 = 484 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 46 = 486 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 48 = 487 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 50 = 488 MHz (when in 3x mode)
			// @ 0x6E div_ref = 1 div7_0 = 60 = 491 MHz (when in 3x mode)


			// @ 0x6E div_ref = 3 div7_0 = 100 = 487 MHz (when in 3x mode)
			// @ 0x6E div_ref = 3 div7_0 = 120 = 490.5 MHz (when in 3x mode)

			// @ 0x6E div_ref = 2 div7_0 = 120 = 493 MHz (when in 3x mode)
/// CHANGE DHCGP = 0 & DCUR=3
			// @ 0x6E div_ref = 2 div7_0 = 120 = 496 MHz (when in 3x mode)
			// @ 0x6E div_ref = 2 div7_0 = 80 = 490 MHz (when in 3x mode)
			// @ 0x6E div_ref = 2 div7_0 = 60 = 426 MHz (when in 3x mode)

		//div_ref: 20+#*5
			// @ 0x6E div_ref = 3 div7_0 = 60 = 320 MHz (when in 3x mode)
			// @ 0x6E div_ref = 3 div7_0 = 92 = 480 MHz (when in 3x mode) (Actually locks)
			// @ 0x6E div_ref = 3 div7_0 = 96 = 500 MHz (when in 3x mode) (ALMOST locks)
			// @ 0x6E div_ref = 9 div7_0 = 246 = 500 MHz (when in 3x mode)
		//Trying dcur = 2
			// @ 0x6E div_ref = 9 div7_0 = 248 = 504 MHz (when in 3x mode)
		//Trying dcur = 2
			// @ 0x6E div_ref = 9 div7_0 = 250 = 508 MHz (when in 3x mode)

			// NOTE TO SELF: Generally as your div_ref gets higher, you can push to higher overall frequencies.
			//

			// @ 0x40, div7_0 = 8 = 40 MHz? 
			// @ 0x80, div7_0 = 8 = 9.3 MHz
			// @ 0x30, div7_0 = 8 = 80 MHz
			// @ 0x3b, div7_0 = 8 = 80 MHz
			// @ 0x39, div7_0 = 8 = 80 MHz
			// @ 0x19, div7_0 = 8 = 40 MHz
			// @ 0x20, div7_0 = 8 = 80 MHz
			// @ 0x2f, div7_0 = 8 = 80 MHz

			// @ 0x69, div7_0 = 27, div_ref = 1 = 103.2 (Works full bore at 307.6MHz)
        dr1 = 0; //No apparent impact?
        dr3 = 3; //No apparent impact?
        dchgp = 0; //No apparent impact?
        dcur = 1;  //No apparent impact?
        REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_MODE_HF, 0x6e);
	}

    uint8_t i2c_bbpll_lref  = (dchgp << I2C_BBPLL_OC_DCHGP_LSB) | (div_ref);
    uint8_t i2c_bbpll_div_7_0 = div7_0;
    uint8_t i2c_bbpll_dcur = (2 << I2C_BBPLL_OC_DLREF_SEL_LSB ) | (1 << I2C_BBPLL_OC_DHREF_SEL_LSB) | dcur;
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_REF_DIV, i2c_bbpll_lref);
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DIV_7_0, i2c_bbpll_div_7_0);
    REGI2C_WRITE_MASK(I2C_BBPLL, I2C_BBPLL_OC_DR1, dr1);
    REGI2C_WRITE_MASK(I2C_BBPLL, I2C_BBPLL_OC_DR3, dr3);
    REGI2C_WRITE(I2C_BBPLL, I2C_BBPLL_OC_DCUR, i2c_bbpll_dcur);

#endif


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

	while(1)
	{
		// Create a nice little pulse train at 1/10th 
		__asm__ __volatile__ ("set_bit_gpio_out 0x1");
		__asm__ __volatile__ ("nop\nnop\nnop\nnop");

		NOP5
		NOP10 NOP10 NOP10 NOP10 

		__asm__ __volatile__ ("clr_bit_gpio_out 0x1");
		// Loop takes 4 nop's.

		NOP10 NOP10 NOP10 NOP10 
		NOP5
	}


	ESP_LOGI( "sandbox", "Main Done on %d", GPIO_NUM_16 );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
	//uint32_t buffer[4] = { 0 };
	//int r = esp_flash_read( 0, buffer, 0x3f0000, sizeof( buffer ) );
//	ESP_LOGI( "sandbox", "global_i: %d", global_i++ );

	global_i++;	
	//ESP_LOGI( "sandbox", "At %d", global_i );

}

swadgeMode sandbox_mode =
{
    .modeName = "sandbox",
    .fnEnterMode = sandbox_main,
    .fnExitMode = sandbox_exit,
    .fnMainLoop = sandbox_tick,
    .fnButtonCallback = NULL,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

