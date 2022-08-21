#include <stdio.h>
#include <string.h>


#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/adc.h"
//#include "esp_adc_cal.h"
#include "esp_log.h"
#include "driver/temp_sensor.h"
#include "esp_check.h"
#include "soc/spi_reg.h"
#include "rom/lldesc.h"
#include "soc/periph_defs.h"
#include "hal/adc_hal_conf.h"
#include "soc/system_reg.h"
#include "hal/gpio_types.h"
#include "soc/system_reg.h"
#include "soc/dport_access.h"
#include "soc/dedic_gpio_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_log.h"
#include "soc/rtc.h"
#include "soc/rtc_io_reg.h"
#include "soc/apb_saradc_reg.h"
#include "soc/sens_reg.h"
#include "hal/adc_ll.h"
#include "driver/rtc_io.h"
#include "soc/spi_reg.h"


#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"

#include "soc/dedic_gpio_reg.h"

#include "swadgeMode.h"
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"
#include "meleeMenu.h"

static lldesc_t dma1 = {0};
static lldesc_t dma2 = {0};

volatile int global_i = 100;
meleeMenu_t * menu;
font_t meleeMenuFont;
display_t * disp;
volatile int doexit;

//#define REBOOT_TEST
//#define PROFILE_TEST
#define REG_LEDC_TEST


// Example to do true inline assembly.  This will actually compile down to be
// included in the code, itself, and "should" (does in all the tests I've run)
// execute in one clock cycle since there is no function call and rsr only
// takes one cycle to complete. 
static inline uint32_t get_ccount()
{
	uint32_t ccount;
	asm volatile("rsr %0,ccount":"=a" (ccount));
	return ccount;
}


#define SAR_SIMPLE_NUM  1024  // Set sample number of enabled unit.

// Use two DMA linker to save ADC data. ADC sample 1 times -> 2 byte data -> 2 DMA link buf.
#define SAR_DMA_DATA_SIZE(unit, sample_num)	 	(SAR_EOF_NUMBER(unit, sample_num))
#define SAR_EOF_NUMBER(unit, sample_num)		((sample_num) * (unit))
#define SAR_MEAS_LIMIT_NUM(unit, sample_num)	(SAR_SIMPLE_NUM)
#define SAR_SIMPLE_TIMEOUT_MS  1000

static uint16_t link_buf[2][SAR_DMA_DATA_SIZE(1, SAR_SIMPLE_NUM)] = {0};
static uint16_t safe_buf[SAR_DMA_DATA_SIZE(1, SAR_SIMPLE_NUM)];
intr_handle_t adc_dma_isr_handle;
 
int inttest1 = 0, inttest2 = 0;
int readok = 0;
/** ADC-DMA ISR handler. */
static IRAM_ATTR void adc_dma_isr(void *arg)
{
	uint32_t int_st = REG_READ(SPI_DMA_INT_ST_REG(3));
	int task_awoken = pdFALSE;
	REG_WRITE(SPI_DMA_INT_CLR_REG(3), int_st);

	if (int_st & SPI_IN_SUC_EOF_INT_ST_M) {
		if( dma1.owner == 0 )
		{
			if( readok == 0 )
			{
				memcpy( safe_buf, link_buf[0], sizeof( safe_buf ) );
				readok = 1;
			}
			memset( link_buf[0], 0, sizeof( link_buf[0] ) );
			dma1.owner = 1;
			inttest2++;
		}
		if( dma2.owner == 0 )
		{
			memset( link_buf[1], 0, sizeof( link_buf[1] ) );
			dma2.owner = 1;
		}
		inttest1++;
	}
	if (int_st & SPI_IN_DONE_INT_ST) {
		// Not used.  We want to read for forever.
	}
	if (task_awoken == pdTRUE) {
		portYIELD_FROM_ISR();
	}
}

#define QDELAY { int i; for( i = 0; i < 30; i++ ) __asm__ __volatile__ ("nop\nnop\nnop\nnop\nnop"); }


void menuCb(const char* opt)
{
}

void sandbox_main(display_t * disp_in)
{
	doexit = 0;

//	memset( &meleeMenuFont, 0, sizeof( meleeMenuFont ) );
	menu = 0; disp = disp_in;
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );

	inttest1 = 0;
	inttest2 = 0;

	// For blister
	REG_WRITE( GPIO_ENABLE_W1TC_REG, 1<<(7) );
	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<(7) );
	REG_WRITE( GPIO_PIN7_REG, 0 );
	REG_WRITE( IO_MUX_GPIO7_REG, 3<<FUN_DRV_S | 1<<MCU_SEL_S | 	1<<FUN_PU_S );

	// For mic
	REG_WRITE( GPIO_ENABLE_W1TC_REG, 1<<(8) );
	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<(8) );
	REG_WRITE( GPIO_PIN8_REG, 0 );
	REG_WRITE( IO_MUX_GPIO8_REG, 3<<FUN_DRV_S | 1<<MCU_SEL_S );// | 	1<<FUN_PD_S );



	// This is absolutely rquired.
	// I don't fully understand why, but things have a BAD DAY if you don't have this.
	portDISABLE_INTERRUPTS();


	// Configure the SPI3 DMA
	{
		uint32_t int_mask = SPI_IN_SUC_EOF_INT_ENA;
		uint32_t dma_addr = (uint32_t)&dma1;

		dma1 = (lldesc_t) {
			.size = sizeof(link_buf[0]),
			.owner = 1,
			.buf = (uint8_t*)&link_buf[0][0],
			.qe.stqe_next = &dma2,
		};
		dma2 = (lldesc_t) {
			.size = sizeof(link_buf[1]),
			.owner = 1,
			.buf = (uint8_t*)&link_buf[1][0],
			.qe.stqe_next = &dma1,
		};
		REG_WRITE(SPI_DMA_INT_ENA_REG(3), 0);
		REG_WRITE(SPI_DMA_INT_CLR_REG(3), UINT32_MAX);
		esp_intr_alloc(ETS_SPI3_DMA_INTR_SOURCE, 0, &adc_dma_isr, NULL, &adc_dma_isr_handle );

		REG_CLR_BIT(SPI_DMA_IN_LINK_REG(3), SPI_INLINK_STOP);
		REG_CLR_BIT(DPORT_PERIP_RST_EN_REG, DPORT_SPI3_DMA_RST_M);
		REG_CLR_BIT(DPORT_PERIP_RST_EN_REG, DPORT_SPI3_RST_M);
		REG_WRITE(SPI_DMA_INT_CLR_REG(3), 0xFFFFFFFF); 

		REG_SET_BIT(DPORT_PERIP_CLK_EN_REG, DPORT_APB_SARADC_CLK_EN_M);
		REG_SET_BIT(DPORT_PERIP_CLK_EN_REG, DPORT_SPI3_DMA_CLK_EN_M);
		REG_SET_BIT(DPORT_PERIP_CLK_EN_REG, DPORT_SPI3_CLK_EN);
		REG_CLR_BIT(DPORT_PERIP_RST_EN_REG, DPORT_SPI3_DMA_RST_M);
		REG_CLR_BIT(DPORT_PERIP_RST_EN_REG, DPORT_SPI3_RST_M);
		REG_WRITE(SPI_DMA_INT_CLR_REG(3), 0xFFFFFFFF);
		REG_WRITE(SPI_DMA_INT_ENA_REG(3), int_mask | REG_READ(SPI_DMA_INT_ENA_REG(3)));

		// Not sure why, but you *must* preserve this order.  That includes
		// stopping and starting, twice.
		REG_SET_BIT(SPI_DMA_IN_LINK_REG(3), SPI_INLINK_STOP);
		REG_CLR_BIT(SPI_DMA_IN_LINK_REG(3), SPI_INLINK_START);
		SET_PERI_REG_BITS(SPI_DMA_IN_LINK_REG(3), SPI_INLINK_ADDR, (uint32_t)dma_addr, 0);
		REG_SET_BIT(SPI_DMA_CONF_REG(3), SPI_IN_RST);
		REG_CLR_BIT(SPI_DMA_CONF_REG(3), SPI_IN_RST);
		REG_CLR_BIT(SPI_DMA_IN_LINK_REG(3), SPI_INLINK_STOP);
		REG_SET_BIT(SPI_DMA_IN_LINK_REG(3), SPI_INLINK_START);
	}

	//Reset module.
	CLEAR_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_SAR_I2C_FORCE_PD_M); 
	SET_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_SAR_I2C_FORCE_PU_M);   

	// I think you can mess with this somehow to do temperature sensing.
	// doing the REGI2C_WRITE_MASK calls in the middle seems to wreck things up.
	REGI2C_WRITE_MASK(I2C_SAR_ADC, ADC_SARADC_ENCAL_REF_ADDR, 0);
	REGI2C_WRITE_MASK(I2C_SAR_ADC, ADC_SARADC_ENT_TSENS_ADDR, 0);
	REGI2C_WRITE_MASK(I2C_SAR_ADC, ADC_SARADC_ENT_RTC_ADDR, 0);

	// This appears to be some sort of undocumented register?
	// It seems to force the SAR to power up.
	WRITE_PERI_REG( SENS_SAR_POWER_XPD_SAR_REG,
		SENS_FORCE_XPD_SAR_S<<SENS_FORCE_XPD_SAR_PU );

	// Enable SAR ADC
	SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN0_REG,DPORT_APB_SARADC_CLK_EN);

	{
		// Note APLL max safe frequency is 125MHz It seems 
		// That the APLL (silicon-depdent) can go up to 160MHz.
		
		// APLL_FREQ = XTAL * ( sdm2 + sdm1/256 + sdm0/65536 + 4 ) / (2*(odiv+2))
		const int sdm2 = 8;
		const int sdm1 = 128;
		const int sdm0 = 0;
		const int odiv = 0;
		REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD, 0 );
		REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU, 1 );
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, sdm2);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, sdm0);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, sdm1);
		REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_1);
		REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_2_REV1);
		REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_OR_OUTPUT_DIV, odiv);
	}

		
	WRITE_PERI_REG( APB_SARADC_FILTER_CTRL_REG, 0 ); // Disable filters	

	const int clock_select_source = 1; // Clock source: 1) APLL, 2) APB_CLK (80MHz)
	
	// Setup SAR clocking.  
	// Clock Output = (clkm_div + 1 + clkm_div_a/clkm_div_b) * Clock
	// Note: Cranking this seems to degrade quality.  It's like this doesn't
	// control the SAR frequency, but rather the sampling-of-the-sar frequency
	// a lot like sar_clk_div, quality-wise.
	const int clkm_div = 0;
	const int clkm_div_b = 1;
	const int clkm_div_a = 0;

	// number of adc cycles to wait before SPI DMA reads value.
	// more experimentation is needed but the "correct" value for this is
	// between 15 and 20? There is some sort of gating here. if sar_clk_div == 1
	// TODO: Much more research is needed on this.
	// if sar_clk_div = 4, timer_target = 48
	// if sar_clk_div = 2, timer_target = 28
	// BIG TODO: This should be much more closely investigated!!!! XXX
	const int timer_target = 48*2;
	
	// This seems to have an impact on how fast the SAR samples.  This does not
	// impact the output frame rate that the SPI DMA reads at, but it does impact
	// how quickly that datais ready.  Setting it to 1 makes the results
	// noticably more chunky.  I think 4 is the "right" value for full depth.
	// 0 gives us 7? bits?
	//
	// Reducing this value lets you tighten up timer_target
	const int sar_clk_div = 4*2; // default is 4. (0 is invalid)
	
	// Perf checks.
	// @1302 kSPS APLL = 125MHz, clkm = 0+1, timer_target = 48,
	//		sar_clk_div = 4 = 651k * 2 = 1302kSPS (exactly)
	//

	// Work modes:
	// 0: Single-ADC mode.
	// 1: Double-mode (sync) mode.
	// 2: Alternating mode.
	const int work_mode = 0;
	const int sar_sel = 0; //primary SAR.

	WRITE_PERI_REG( APB_SARADC_APB_ADC_CLKM_CONF_REG,
		clock_select_source<<APB_SARADC_CLK_SEL_S | 	
		clkm_div<<APB_SARADC_CLKM_DIV_NUM_S |
		clkm_div_b<<APB_SARADC_CLKM_DIV_B_S |
		clkm_div_a<<APB_SARADC_CLKM_DIV_A_S );

	WRITE_PERI_REG( APB_SARADC_CTRL2_REG,
		timer_target<<APB_SARADC_TIMER_TARGET_S | //XXX TODO EXPERIMENT
		0<<APB_SARADC_MEAS_NUM_LIMIT_S |
		1<<APB_SARADC_TIMER_SEL_S | // "Reserved" in datasheet.
		0<<APB_SARADC_TIMER_EN_S // Will be enabled later.
		);

	WRITE_PERI_REG( APB_SARADC_CTRL_REG,
		sar_clk_div<<APB_SARADC_SAR_CLK_DIV_S | // default is 4. 0 is invalid.
		0<<APB_SARADC_SAR_CLK_GATED_S | // I don't know what this does.
		3<<APB_SARADC_SAR1_PATT_LEN_S | // Pattern length = 1 (0+1) = 1
		0<<APB_SARADC_SAR2_PATT_LEN_S | // Pattern length = 1 (0+1) = 1
		work_mode<<APB_SARADC_WORK_MODE_S | // Work mode = 2 ( 1 = double-channel mode, 0 for single channel)
		sar_sel<<APB_SARADC_SAR_SEL_S | // Select SAR2 as the primary SAR. (Needs more experimentation)
		1<<APB_SARADC_DATA_SAR_SEL_S | // Use 11-bit encoding (DMA Type II Data so we get IDs of which SAR is which )
		0<<APB_SARADC_START_FORCE_S // Unsure about this. Seems to need to be 0 to work at all.
		);
	
	REG_SET_FIELD(SENS_SAR_MEAS1_CTRL2_REG, SENS_SAR1_EN_PAD, 0xff );
	REG_SET_FIELD(SENS_SAR_MEAS2_CTRL2_REG, SENS_SAR2_EN_PAD, 0xff );
	
	WRITE_PERI_REG(APB_SARADC_SAR1_PATT_TAB1_REG,0x53637303);	// set adc1 channel & bitwidth & atten  
	WRITE_PERI_REG(APB_SARADC_SAR2_PATT_TAB1_REG,0x50ffffff); //set adc2 channel & bitwidth & atten

	// GENERAL NOTE:
	//	SENS_SAR_MEAS1_CTRL2_REG and SENS_SAR_MEAS1_CTRL2_REG should not be used
	//	here because they are for the RTC.

	// Configure DIG ADC CTRL for SPI DMA.
	WRITE_PERI_REG( APB_SARADC_DMA_CONF_REG, 
		1<<APB_SARADC_APB_ADC_TRANS_S |
		(sizeof(link_buf[0])/sizeof(link_buf[0][0]))<<APB_SARADC_APB_ADC_EOF_NUM_S );

	// Hmmm... Why does this seemingly impact SAR2?  Setting to 0 breaks SAR2.
	// I don't actually understand this. But, doing the following two steps 
	// seems to make it possible to actually run both SARs
	// XXX I really don't understand this, but I really believe these two steps
	// are crucial to doing dual SAR1+SAR2 mode reliably.
	REG_WRITE( SENS_SAR_MEAS1_MUX_REG, SENS_SAR1_DIG_FORCE );
	REG_WRITE( SENS_SAR_MEAS2_MUX_REG, 0x00000000 );

	// Actually enable the SAR.
	SET_PERI_REG_MASK(APB_SARADC_CTRL2_REG,APB_SARADC_TIMER_EN);

	portENABLE_INTERRUPTS();









	loadFont("mm.font", &meleeMenuFont);
	menu = initMeleeMenu("Hello", &meleeMenuFont, menuCb);
	addRowToMeleeMenu(menu, "Test 1");
	addRowToMeleeMenu(menu, "Test 2");
	addRowToMeleeMenu(menu, "Test 3");

	ESP_LOGI( "sandbox", "Loaded X" );
}

void sandbox_exit()
{
	doexit = 1;
	ESP_LOGI( "sandbox", "Exit" );
	if( menu )
	{
		deinitMeleeMenu(menu);
		freeFont(&meleeMenuFont);
	}

	if( adc_dma_isr_handle )
		esp_intr_free( adc_dma_isr_handle );

	ESP_LOGI( "sandbox", "Exit" );
}


void sandbox_tick()
{

	
	ESP_LOGI( "sandbox", "global_i: %d %04x %04x %04x %04x", inttest2, safe_buf[0], safe_buf[1], safe_buf[2], safe_buf[3] );
	readok = 0;

	if( menu )
	    drawMeleeMenu(disp, menu);

}

void sandbox_button(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            case DOWN:
            case LEFT:
            case RIGHT:
            case BTN_A:
            {
                meleeMenuButton(menu, evt->button);
                break;
            }
            case START:
            case SELECT:
            case BTN_B:
            default:
            {
                break;
            }
        }
    }
}

swadgeMode sandbox_mode =
{
    .modeName = "sandbox",
    .fnEnterMode = sandbox_main,
    .fnExitMode = sandbox_exit,
    .fnMainLoop = sandbox_tick,
    .fnButtonCallback = sandbox_button,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};
