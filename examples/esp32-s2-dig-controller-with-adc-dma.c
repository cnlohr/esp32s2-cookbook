/* 
	ESP32S2 DIG ADC example with DMA using SPI2.
	
	Based on examples found in the IDF.
	
	Note: There is something janky about SAR ADC (DIG) 1. 2 Seems fine.
	also, I think this is sampling at 1msps/s.

*/
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
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

static lldesc_t dma1 = {0};
static lldesc_t dma2 = {0};

#define SAR_SIMPLE_NUM  1024  // Set sample number of enabled unit.

// Use two DMA linker to save ADC data. ADC sample 1 times -> 2 byte data -> 2 DMA link buf.
#define SAR_DMA_DATA_SIZE(unit, sample_num)	 	(SAR_EOF_NUMBER(unit, sample_num))
#define SAR_EOF_NUMBER(unit, sample_num)		((sample_num) * (unit))
#define SAR_MEAS_LIMIT_NUM(unit, sample_num)	(SAR_SIMPLE_NUM)
#define SAR_SIMPLE_TIMEOUT_MS  1000


typedef struct adc_dac_dma_isr_handler_ {
	uint32_t mask;
	intr_handler_t handler;
	void* handler_arg;
	SLIST_ENTRY(adc_dac_dma_isr_handler_) next;
} adc_dac_dma_isr_handler_t;


typedef struct dma_msg {
	uint32_t int_msk;
	uint8_t *data;
	uint32_t data_len;
} adc_dma_event_t;

static uint16_t link_buf[2][SAR_DMA_DATA_SIZE(1, SAR_SIMPLE_NUM)] = {0};

intr_handle_t adc_dma_isr_handle;
 
int inttest1, inttest2;
/** ADC-DMA ISR handler. */
static IRAM_ATTR void adc_dma_isr(void *arg)
{
	uint32_t int_st = REG_READ(SPI_DMA_INT_ST_REG(3));
	int task_awoken = pdFALSE;
	REG_WRITE(SPI_DMA_INT_CLR_REG(3), int_st);
	if (int_st & SPI_IN_SUC_EOF_INT_ST_M) {
		inttest1++;
	}
	if (int_st & SPI_IN_DONE_INT_ST) {
		inttest2++;
	}
	if (task_awoken == pdTRUE) {
		portYIELD_FROM_ISR();
	}
}

#define QDELAY { int i; for( i = 0; i < 30; i++ ) __asm__ __volatile__ ("nop\nnop\nnop\nnop\nnop"); }

void app_main(void)
{
	printf("Hello world!\n");

	/* Print chip information */
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
			CONFIG_IDF_TARGET,
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	printf("silicon revision %d, ", chip_info.revision);

	printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

	// I think you can mess with this somehow to do temperature sensing.
	// doing the REGI2C_WRITE_MASK calls in the middle seems to wreck things up.
	REGI2C_WRITE_MASK(I2C_SAR_ADC, ADC_SARADC_ENCAL_REF_ADDR, 0);
	REGI2C_WRITE_MASK(I2C_SAR_ADC, ADC_SARADC_ENT_TSENS_ADDR, 0);
	REGI2C_WRITE_MASK(I2C_SAR_ADC, ADC_SARADC_ENT_RTC_ADDR, 0);


	{
		// ADC
		// ADC1_0 is RTC_GPIO1 or GPIO1.
		gpio_config_t io_conf = { .mode=GPIO_MODE_DISABLE, .pin_bit_mask=(1ULL<<GPIO_NUM_6) };
		ESP_ERROR_CHECK(gpio_config(&io_conf));
		rtc_gpio_init( GPIO_NUM_6 );
	}

	DPORT_REG_WRITE( GPIO_PIN_MUX_REG[6], FUN_IE | SLP_IE ); // Configure GPIO (disable IE, PUs, etc.)
	DPORT_REG_WRITE( RTC_GPIO_OUT_W1TC_REG, (1<<(10+6)) );
	DPORT_REG_WRITE( RTC_IO_TOUCH_PAD6_REG, RTC_IO_TOUCH_PAD1_FUN_IE | RTC_IO_TOUCH_PAD1_SLP_IE );
	
   // Enable 8M clock source for RNG (this is actually enough to produce strong random results,
	// but enabling the SAR ADC as well adds some insurance.)
	REG_SET_BIT(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_DIG_CLK8M_EN);
	REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, 0 ); // Disable divisor on CK8M (TODO) TODO TODO Seems to have no impact.
	REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL, 0 ); // Use 10MHz signal.

	// Configure the SPI3 DMA

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
	REG_SET_BIT(SPI_DMA_CONF_REG(3), SPI_INLINK_AUTO_RET);


	// This is absolutely rquired.
	// I don't fully understand why, but things have a BAD DAY if you don't have this.
	portDISABLE_INTERRUPTS();

	//Reset module.
	CLEAR_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_SAR_I2C_FORCE_PD_M); 
	SET_PERI_REG_MASK(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_SAR_I2C_FORCE_PU_M);   

	CLEAR_PERI_REG_MASK(APB_SARADC_CTRL2_REG,APB_SARADC_TIMER_EN);
		
	//Not sure why I need to double set?
	REG_SET_BIT( SENS_SAR_MEAS1_CTRL1_REG, SENS_RTC_CLKGATE_EN );
	REG_SET_BIT( SENS_SAR_MEAS1_CTRL1_REG, SENS_RTC_CLKGATE_EN );
//	SENS.sar_meas1_ctrl1.rtc_saradc_clkgate_en = 1;
//	SENS.sar_power_xpd_sar.force_xpd_sar = SENS_FORCE_XPD_SAR_PU;
	REG_SET_FIELD( SENS_SAR_POWER_XPD_SAR_REG, SENS_FORCE_XPD_SAR, SENS_FORCE_XPD_SAR_PU );
	REG_SET_FIELD( SENS_SAR_POWER_XPD_SAR_REG, SENS_FORCE_XPD_SAR, SENS_FORCE_XPD_SAR_PU );


	// Enable SAR ADC to read a disconnected input for additional entropy
	SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN0_REG,DPORT_APB_SARADC_CLK_EN);

		
	// Make APLL 125MHz (the highest it can safely be)
	REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD, 0 );
	REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU, 1 );
	REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, 8);
	REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, 0);
	REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, 128);
	REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_1);
	REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_2_REV1);
	REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_OR_OUTPUT_DIV, 0);

	// Clock source: 1) APLL, 2) APB_CLK
	REG_SET_FIELD(APB_SARADC_APB_ADC_CLKM_CONF_REG, APB_SARADC_CLK_SEL, 1);
	REG_SET_FIELD(APB_SARADC_APB_ADC_CLKM_CONF_REG, APB_SARADC_CLKM_DIV_NUM, 0);
	REG_SET_FIELD(APB_SARADC_APB_ADC_CLKM_CONF_REG, APB_SARADC_CLKM_DIV_B, 0);
	REG_SET_FIELD(APB_SARADC_APB_ADC_CLKM_CONF_REG, APB_SARADC_CLKM_DIV_A, 0);
	REG_SET_FIELD(APB_SARADC_CTRL2_REG, APB_SARADC_TIMER_TARGET, 60);
	REG_SET_FIELD(APB_SARADC_CTRL_REG, APB_SARADC_SAR_CLK_DIV, 4 ); // default is 4.

	// I think this means our samplerate is 125 MHz / 60 / (4+1) = 520kSPS?
	// Something is amiss.  I think our actual rate is twice that.

	REG_SET_FIELD(SENS_SAR_MEAS1_CTRL2_REG, SENS_SAR1_EN_PAD, 0xff );
	REG_SET_FIELD(SENS_SAR_MEAS2_CTRL2_REG, SENS_SAR2_EN_PAD, 0xff );
	SET_PERI_REG_MASK(SENS_SAR_MEAS2_CTRL2_REG, SENS_SAR2_EN_PAD_FORCE );

	REG_SET_FIELD(APB_SARADC_CTRL_REG, APB_SARADC_SAR1_PATT_LEN, 0);
	WRITE_PERI_REG(APB_SARADC_SAR1_PATT_TAB1_REG,0x53ffffff);	// set adc1 channel & bitwidth & atten  

	REG_SET_FIELD(APB_SARADC_CTRL_REG, APB_SARADC_SAR2_PATT_LEN, 0);
	WRITE_PERI_REG(APB_SARADC_SAR2_PATT_TAB1_REG,0x5fffffff); //set adc2 channel & bitwidth & atten

	// Set this to 1 for double-channel mode or 2 for alternate-scan mode. (0 for single-channel)
	REG_SET_FIELD(APB_SARADC_CTRL_REG, APB_SARADC_WORK_MODE, 1);
	//XXX For some reason SAR1 is much slower than SAR2?  Or something about SAR1 breaks a lot.
	// Recommend using SAR2 for boundary tests.
	REG_SET_FIELD(APB_SARADC_CTRL_REG, APB_SARADC_SAR_SEL, 1 ); 
	CLEAR_PERI_REG_MASK(APB_SARADC_CTRL2_REG, APB_SARADC_MEAS_NUM_LIMIT);
	
	// Try 11-bit encoding.  This should set the ADC to Type II DMA Output
	// which tags each 16-bit sample with which ADC and channel it was reading from.
	SET_PERI_REG_MASK( APB_SARADC_CTRL_REG, APB_SARADC_DATA_SAR_SEL );
	

	// Configure DIG ADC CTRL for SPI DMA
	REG_SET_FIELD(APB_SARADC_DMA_CONF_REG, APB_SARADC_APB_ADC_EOF_NUM, sizeof(link_buf[0])/sizeof(link_buf[0][0]) );
	SET_PERI_REG_MASK(APB_SARADC_DMA_CONF_REG, APB_SARADC_APB_ADC_TRANS);

	// This appears to be some sort of undocumented register?  Not sure its purpose.
	REG_SET_FIELD(SENS_SAR_POWER_XPD_SAR_REG, SENS_FORCE_XPD_SAR, 3);
	
	// this code replaces the adc_ll_set_controller( ADC_NUM_1, ADC_LL_CTRL_DIG ); 
	//	function call.
	SET_PERI_REG_MASK(SENS_SAR_MEAS1_MUX_REG,SENS_SAR1_DIG_FORCE);
	SET_PERI_REG_MASK(SENS_SAR_MEAS1_CTRL2_REG,SENS_MEAS1_START_FORCE);
	SET_PERI_REG_MASK(SENS_SAR_MEAS1_CTRL2_REG,SENS_SAR1_EN_PAD_FORCE);


	// Unsure why I need to do this twice, otherwise things get janky
	// and I think? I start reading the wrong ADC or something?
	CLEAR_PERI_REG_MASK(APB_SARADC_CTRL_REG,APB_SARADC_START_FORCE);
	SET_PERI_REG_MASK(APB_SARADC_CTRL2_REG,APB_SARADC_TIMER_EN);
	SET_PERI_REG_MASK(APB_SARADC_CTRL2_REG, APB_SARADC_TIMER_SEL);
	SET_PERI_REG_MASK(APB_SARADC_CTRL2_REG,APB_SARADC_TIMER_EN);


	WRITE_PERI_REG( APB_SARADC_FILTER_CTRL_REG, 0 ); // Disable filters
	portENABLE_INTERRUPTS();

	for (int i = 1000; i >= 0; i--)
	{
		printf("Restarting in %d seconds... %08x %08x %04x %04x %04x %04x %d %d\n", i, READ_PERI_REG(SENS_SAR_MEAS1_CTRL2_REG), READ_PERI_REG( APB_SARADC_CTRL_REG ), link_buf[0][0], link_buf[0][1], link_buf[0][2], link_buf[0][3], inttest1, inttest2 );
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	printf("Restarting now.\n");
	fflush(stdout);
	esp_restart();
}
