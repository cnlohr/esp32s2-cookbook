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
#include "gpio_sig_map.h"

/*
	A realy bad but functional example of outputting I2S on the ESP32S2
*/

int global_i = 100;


static inline uint32_t getCycleCount()
{
    uint32_t ccount;
    asm volatile("rsr %0,ccount":"=a" (ccount));
    return ccount;
}



#define SWIO_PIN        6
#define SWCLK_PIN       4
#define SWIO_PU_PIN     9
#define SWCLK_PU_PIN    8
#define VDD5V_EN        12
#define VDD3V3_EN       11
#define MULTI2_PIN      2
#define WSO_PIN         14


#define IO_MUX_REG(x) XIO_MUX_REG(x)
#define XIO_MUX_REG(x) IO_MUX_GPIO##x##_REG

#define GPIO_NUM(x) XGPIO_NUM(x)
#define XGPIO_NUM(x) GPIO_NUM_##x




#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/i2s_reg.h"
#include "soc/periph_defs.h"
#include "rom/lldesc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "regi2c_apll.h"
#include "regi2c_ctrl_ll.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/regi2c_ctrl.h"
#include "hal/clk_tree_ll.h"

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

void apll_quick_update( uint32_t sdm )
{
	//REGI2C_WRITE(I2C_APLL, I2C_APLL_DSDM2, sdm>>16);
	//REGI2C_WRITE(I2C_APLL, I2C_APLL_DSDM0, (sdm&0xff));
	//REGI2C_WRITE(I2C_APLL, I2C_APLL_DSDM1, (sdm>>8)&0xff);
	regi2c_write_reg_raw(I2C_APLL, I2C_APLL_HOSTID, I2C_APLL_DSDM2, sdm>>16);
	regi2c_write_reg_raw(I2C_APLL, I2C_APLL_DSDM0, I2C_APLL_DSDM0, (sdm&0xff));
	regi2c_write_reg_raw(I2C_APLL, I2C_APLL_DSDM1, I2C_APLL_DSDM1, (sdm>>8)&0xff);

}



void sandbox_main()
{
	uprintf( "sandbox_main()\n" );

					// Output clock on P2.

					// Maximize the drive strength.
					gpio_set_drive_capability( GPIO_NUM(MULTI2_PIN), GPIO_DRIVE_CAP_3 );

					// Use the IO matrix to create the inverse of TX on pin 17.
					gpio_matrix_out( GPIO_NUM(MULTI2_PIN), CLK_I2S_MUX_IDX, 1, 0 );

					periph_module_enable(PERIPH_I2S0_MODULE);

					int use_apll = 1;
					int sdm0 = 100;
					int sdm1 = 230;
					int sdm2 = 8;
					int odiv = 0;

					local_rtc_clk_apll_enable( use_apll, sdm0, sdm1, sdm2, odiv );

					if( use_apll )
					{
						WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (1<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (1<<I2S_CLKM_DIV_NUM_S) );
					}
					else
					{
						// fI2S = fCLK / ( N + B/A )
						// DIV_NUM = N
						// Note I2S_CLKM_DIV_NUM minimum = 2 by datasheet.  Less than that and it will ignoreeee you.
						WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (2<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (1<<I2S_CLKM_DIV_NUM_S) );  // Minimum reduction, 2:1
					}

}


uint32_t frame = 0;
void sandbox_tick()
{
	//uprintf( "%08x\n", REGI2C_READ(I2C_APLL, I2C_APLL_DSDM2 ));
	// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );\n
int jj;
for( jj = 0; jj < 33; jj ++ )
{
	// 3.25 is the harmonic
	// Why 3.25??? I have NO IDEA
	// I was just experimenting and 3.25 is loud.
	float fTarg = (903.9)/13.0;

	// We are actually / /4 in reality. Because of the hardware divisors in the chip.

	uint32_t codeTarg = fTarg * 65536 * 4;


	int fplv = 0;
	frame+=6;
	//if( frame > 2000 ) frame -= 2000;
	if( frame & 0x400 )
	{
		fplv = frame & 0x3ff;
	}
	else
	{
		fplv = 0x3ff - (frame & 0x3ff);
	}

	fplv *= 4;


	codeTarg += fplv;

	uint32_t sdm = (codeTarg / 40 * 2 - 4 * 65536);
	apll_quick_update( sdm );
}
//	vTaskDelay( 1 );
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnAdvancedUSB = NULL
};
