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

#include "siggen.h"




#define I2C_RTC_WIFI_CLK_EN (SYSCON_WIFI_CLK_EN_REG)

#define I2C_RTC_CLK_GATE_EN    (BIT(18))
#define I2C_RTC_CLK_GATE_EN_M  (BIT(18))
#define I2C_RTC_CLK_GATE_EN_V  0x1
#define I2C_RTC_CLK_GATE_EN_S  18

#define I2C_RTC_CONFIG0  0x6000e048

#define I2C_RTC_MAGIC_CTRL 0x00001FFF
#define I2C_RTC_MAGIC_CTRL_M  ((I2C_RTC_MAGIC_CTRL_V)<<(I2C_RTC_MAGIC_CTRL_S))
#define I2C_RTC_MAGIC_CTRL_V  0x1FFF
#define I2C_RTC_MAGIC_CTRL_S  4

#define I2C_RTC_CONFIG1  0x6000e044

#define I2C_RTC_BOD_MASK (BIT(22))
#define I2C_RTC_BOD_MASK_M  (BIT(22))
#define I2C_RTC_BOD_MASK_V  0x1
#define I2C_RTC_BOD_MASK_S  22

#define I2C_RTC_SAR_MASK (BIT(18))
#define I2C_RTC_SAR_MASK_M  (BIT(18))
#define I2C_RTC_SAR_MASK_V  0x1
#define I2C_RTC_SAR_MASK_S  18

#define I2C_RTC_BBPLL_MASK (BIT(17))
#define I2C_RTC_BBPLL_MASK_M  (BIT(17))
#define I2C_RTC_BBPLL_MASK_V  0x1
#define I2C_RTC_BBPLL_MASK_S  17

#define I2C_RTC_APLL_MASK (BIT(14))
#define I2C_RTC_APLL_MASK_M  (BIT(14))
#define I2C_RTC_APLL_MASK_V  0x1
#define I2C_RTC_APLL_MASK_S  14

#define I2C_RTC_ALL_MASK 0x00007FFF
#define I2C_RTC_ALL_MASK_M  ((I2C_RTC_ALL_MASK_V)<<(I2C_RTC_ALL_MASK_S))
#define I2C_RTC_ALL_MASK_V  0x7FFF
#define I2C_RTC_ALL_MASK_S  8

#define I2C_RTC_CONFIG2  0x6000e000

#define I2C_RTC_BUSY (BIT(25))
#define I2C_RTC_BUSY_M  (BIT(25))
#define I2C_RTC_BUSY_V  0x1
#define I2C_RTC_BUSY_S  25

#define I2C_RTC_WR_CNTL (BIT(24))
#define I2C_RTC_WR_CNTL_M  (BIT(24))
#define I2C_RTC_WR_CNTL_V  0x1
#define I2C_RTC_WR_CNTL_S  24

#define I2C_RTC_DATA 0x000000FF
#define I2C_RTC_DATA_M  ((I2C_RTC_DATA_V)<<(I2C_RTC_DATA_S))
#define I2C_RTC_DATA_V  0xFF
#define I2C_RTC_DATA_S  16

#define I2C_RTC_ADDR 0x000000FF
#define I2C_RTC_ADDR_M  ((I2C_RTC_ADDR_V)<<(I2C_RTC_ADDR_S))
#define I2C_RTC_ADDR_V  0xFF
#define I2C_RTC_ADDR_S  8

#define I2C_RTC_SLAVE_ID 0x000000FF
#define I2C_RTC_SLAVE_ID_M  ((I2C_RTC_SLAVE_ID_V)<<(I2C_RTC_SLAVE_ID_S))
#define I2C_RTC_SLAVE_ID_V  0xFF
#define I2C_RTC_SLAVE_ID_S  0

#define I2C_RTC_MAGIC_DEFAULT (0x1c40)

#define I2C_BOD     0x61
#define I2C_BBPLL   0x66
#define I2C_SAR_ADC 0X69
#define I2C_APLL    0X6D


// Configures APLL = 480 / 4 = 120
// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );
// Datasheet recommends that numerator does not exceed 500MHz.
void IRAM_ATTR local_rtc_clk_apll_enable(bool enable, uint32_t sdm0, uint32_t sdm1, uint32_t sdm2, uint32_t o_div)
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


void IRAM_ATTR regi2c_write_reg_raw_local(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data)
{
    uint32_t temp = ((block & I2C_RTC_SLAVE_ID_V) << I2C_RTC_SLAVE_ID_S)
                    | ((reg_add & I2C_RTC_ADDR_V) << I2C_RTC_ADDR_S)
                    | ((0x1 & I2C_RTC_WR_CNTL_V) << I2C_RTC_WR_CNTL_S)
                    | (((uint32_t)data & I2C_RTC_DATA_V) << I2C_RTC_DATA_S);
    while (REG_GET_BIT(I2C_RTC_CONFIG2, I2C_RTC_BUSY));
    REG_WRITE(I2C_RTC_CONFIG2, temp);
}


void apll_quick_update( uint32_t sdm )
{
	uint8_t sdm2 = sdm>>16;
	uint8_t sdm1 = (sdm>>8)&0xff;
	uint8_t sdm0 = (sdm>>0)&0xff;
	static int last_sdm_0 = -1;
	static int last_sdm_1 = -1;
	static int last_sdm_2 = -1;

	if( sdm2 != last_sdm_2 )
		regi2c_write_reg_raw_local(I2C_APLL, I2C_APLL_HOSTID, I2C_APLL_DSDM2, sdm2);
	if( sdm0 != last_sdm_0 ) 
		regi2c_write_reg_raw_local(I2C_APLL, I2C_APLL_HOSTID, I2C_APLL_DSDM0, sdm0);
	if( sdm1 != last_sdm_1 )
		regi2c_write_reg_raw_local(I2C_APLL, I2C_APLL_HOSTID, I2C_APLL_DSDM1, sdm1);

	last_sdm_2 = sdm2;
	last_sdm_1 = sdm1;
	last_sdm_0 = sdm0;
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

	SigSetupTest(  );
}



#define DisableISR()            do { XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL); portbenchmarkINTERRUPT_DISABLE(); } while (0)
#define EnableISR()             do { portbenchmarkINTERRUPT_RESTORE(0); XTOS_SET_INTLEVEL(0); } while (0)


uint32_t frame = 0;
void sandbox_tick()
{
	//uprintf( "%08x\n", REGI2C_READ(I2C_APLL, I2C_APLL_DSDM2 ));
	// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );\n

	// 13rd harmonic.  
	const float fRadiator = 903.9;
	const float fBandwidth = .125;
	const float fHarmonic = 13.0;

	const float fXTAL = 40;

	const float fTarg = (fRadiator-(fBandwidth/2))/fHarmonic;
	const float fAPLL = fTarg * 2;
	const uint32_t sdmBaseTarget = ( fAPLL * 4 / fXTAL - 4 ) * 65536 + 0.5; // ~649134
	
	
	// We are actually / /4 in reality. Because of the hardware divisors in the chip.
	// 65536 = SDM0's impact.

	// Xtal is at 40 MHz.

	// Target range: 649159 - 649281  (122 sweep)

	const float fRange = (fBandwidth)/fHarmonic;
	const float fAPLLRange = fRange * 2;
	const float sdmRange = ( fAPLLRange * 4 / fXTAL ) * 65536; // ~126

	// 491520 clocks per chip @ SF8 (2.048ms per chirp)
	const uint32_t sdmDivisor = ( CHIPSSPREAD / sdmRange ) + 0.5;

#if 0
	// For DEBUGGING ONLY
	int k;
//DisableISR();
	for( k = 0; k < 33; k++ )
	{
		int fplv = 0;

		// Send every second
		// If you want to dialate time, do it here. 
		frame = (getCycleCount()/200) % 24000000;
		fplv = SigGen( frame, codeTarg );
		uint32_t codeTargUse = codeTarg + fplv;
		uint32_t sdm = (codeTargUse * 2 / 40 - 4 * 65536);  //XXX WARNING CHARLES, why does this move in pairs?
		apll_quick_update( sdm );
		if( fplv < 0 ) break;

		if( fplv > 0 )
			uprintf( "%d %d\n", (int)sdm, (int)fplv );
	}
//EnableISR();
#else

	frame = (getCycleCount()) % 40000000;
	if( frame < 1000000 )
	{
		SigSetupTest();
		DisableISR();
		int iterct = 0;
		while(1)
		{
			int fplv = 0;
			// Send every second
			// If you want to dialate time, do it here. 
			frame = (getCycleCount()) % 40000000;
			fplv = SigGen( frame, sdmBaseTarget );
			uint32_t sdm = sdmBaseTarget + fplv / sdmDivisor;
			apll_quick_update( sdm );
			if( fplv < 0 ) break;
			iterct++;
		}
		EnableISR();
		uprintf( "Iter: %d\n", iterct );
	}
#endif

//	vTaskDelay( 1 );
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnAdvancedUSB = NULL
};
