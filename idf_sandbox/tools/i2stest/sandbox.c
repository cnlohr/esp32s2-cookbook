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

#define BUFF_SIZE_BYTES 2048


#define MAX_DMX 1024

uint32_t * i2sbuffer[4] __attribute__((aligned(128)));
uint8_t dmx_buffer[1024 + 4] = { 0 };
volatile int highest_send = 0;
static intr_handle_t i2s_intr_handle;


int isr_countOut;
int i2s_running;


// On the ESP32S2 (TRM, Page 288)
#define ETS_I2S0_INUM 35
static void IRAM_ATTR i2s_isr_fast(void* arg) {
	REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
	++isr_countOut;
}
static lldesc_t s_dma_desc[4];
static esp_err_t dma_desc_init()
{
	for (int i = 0; i < 4; ++i) {
		i2sbuffer[i] = malloc( BUFF_SIZE_BYTES );
		s_dma_desc[i].length = BUFF_SIZE_BYTES;	 // size of a single DMA buf
		s_dma_desc[i].size = BUFF_SIZE_BYTES;	   // total size of the chain
		s_dma_desc[i].owner = 1;
		s_dma_desc[i].sosf = 1;
		s_dma_desc[i].buf = (uint8_t*) i2sbuffer[i];
		s_dma_desc[i].offset = i;
		s_dma_desc[i].empty = 0;
		s_dma_desc[i].eof = 1;
		s_dma_desc[i].qe.stqe_next = &s_dma_desc[(i+1)&1];
		memset( i2sbuffer[i], 0xff, BUFF_SIZE_BYTES );
	}
	return ESP_OK;
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


static void i2s_fill_buf_fast() {

	ESP_INTR_DISABLE(ETS_I2S0_INUM);
	SET_PERI_REG_BITS(I2S_OUT_LINK_REG(0), I2S_OUTLINK_ADDR_V, ((uint32_t) &s_dma_desc), I2S_OUTLINK_ADDR_S);
	SET_PERI_REG_BITS(I2S_OUT_LINK_REG(0), I2S_OUTLINK_START_V, 1, I2S_OUTLINK_START_S);

	REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);

	SET_PERI_REG_BITS(I2S_FIFO_CONF_REG(0), I2S_DSCR_EN_V, 1, I2S_DSCR_EN_S);

	SET_PERI_REG_BITS(I2S_LC_CONF_REG(0), I2S_OUT_DATA_BURST_EN_V, 1, I2S_OUT_DATA_BURST_EN_S);
	SET_PERI_REG_BITS(I2S_LC_CONF_REG(0), I2S_CHECK_OWNER_V, 1, I2S_CHECK_OWNER_S);
	SET_PERI_REG_BITS(I2S_LC_CONF_REG(0), I2S_OUT_EOF_MODE_V, 1, I2S_OUT_EOF_MODE_S);
	SET_PERI_REG_BITS(I2S_LC_CONF_REG(0), I2S_OUTDSCR_BURST_EN_V, 1, I2S_OUTDSCR_BURST_EN_S);
	SET_PERI_REG_BITS(I2S_LC_CONF_REG(0), I2S_OUT_DATA_BURST_EN_V, 1, I2S_OUT_DATA_BURST_EN_S);


	REG_WRITE(I2S_CONF_REG(0), REG_READ(I2S_CONF_REG(0)) & 0xfffffff0);
	(void) REG_READ(I2S_CONF_REG(0));
	REG_WRITE(I2S_CONF_REG(0), (REG_READ(I2S_CONF_REG(0)) & 0xfffffff0) | 0xf);
	(void) REG_READ(I2S_CONF_REG(0));
	REG_WRITE(I2S_CONF_REG(0), REG_READ(I2S_CONF_REG(0)) & 0xfffffff0);
  //  while (GET_PERI_REG_BITS2(I2S_STATE_REG(0), 0x1, I2S_TX_FIFO_RESET_BACK_S));
	ets_delay_us(10);
	SET_PERI_REG_BITS(I2S_INT_ENA_REG(0), I2S_OUT_DONE_INT_ENA_V, 1, I2S_OUT_DONE_INT_ENA_S);
	ESP_INTR_ENABLE(ETS_I2S0_INUM);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_TX_START_V, 1,I2S_TX_START_S);

}









void sandbox_main()
{
	uprintf( "sandbox_main()\n" );


	dma_desc_init();

	gpio_num_t pins[] = {
		GPIO_NUM_4,
		GPIO_NUM_16,
		GPIO_NUM_17,
		GPIO_NUM_18,
	};

	gpio_config_t conf = {
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	for (int i = 0; i < sizeof(pins)/sizeof(gpio_num_t); ++i) {
		conf.pin_bit_mask = 1LL << pins[i];
		gpio_config(&conf);
	}
	// Maximize the drive strength.
	gpio_set_drive_capability( GPIO_NUM_4, GPIO_DRIVE_CAP_3 );
	gpio_set_drive_capability( GPIO_NUM_16, GPIO_DRIVE_CAP_3 );
	gpio_set_drive_capability( GPIO_NUM_17, GPIO_DRIVE_CAP_3 );
	gpio_set_drive_capability( GPIO_NUM_18, GPIO_DRIVE_CAP_3 );

	// Use the IO matrix to create the inverse of TX on pin 17.
	gpio_matrix_out( GPIO_NUM_16, CLK_I2S_MUX_IDX, 1, 0 );
	gpio_matrix_out( GPIO_NUM_4, I2S0O_BCK_OUT_IDX, 1, 0 );

	// Use the IO matrix to create the inverse of TX on pin 17.
	gpio_matrix_out( GPIO_NUM_17, I2S0O_WS_OUT_IDX, 1, 0 );
	gpio_matrix_out( GPIO_NUM_18, I2S0O_DATA_OUT23_IDX, 1, 0 );

	periph_module_enable(PERIPH_I2S0_MODULE);


	WRITE_PERI_REG( I2S_LC_CONF_REG(0), 1<<I2S_IN_RST_S | 1<<I2S_AHBM_RST_S | 1<<I2S_AHBM_FIFO_RST_S );
	WRITE_PERI_REG( I2S_LC_CONF_REG(0), 0);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), 0x1, 1, I2S_TX_RESET_S);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), 0x1, 0, I2S_TX_RESET_S);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), 0x1, 1, I2S_TX_FIFO_RESET_S);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), 0x1, 0, I2S_TX_FIFO_RESET_S);

	SET_PERI_REG_BITS(I2S_CONF_REG(0), 0x1, 0, I2S_TX_SLAVE_MOD_S);  //Needed, otherwise it waits for a clock.

	WRITE_PERI_REG(I2S_CONF2_REG(0), 0 );

	// Configures APLL = 480 / 4 = 120
	// 40 * (SDM2 + SDM1/(2^8) + SDM0/(2^16) + 4) / ( 2 * (ODIV+2) );
	// Datasheet recommends that numerator does not exceed 500MHz.
	//local_rtc_clk_apll_enable( 1, 0, 0, 8, 0 );

	// According to TRM I2S_TX_BCK_DIV_NUM must not be set to 1.  But that's the only way to get this I2S to do anything fun.
	// In case you want to go fast, be sure to set it to 1!  Then you can output at up to 80MHz with the 160M clock.
	WRITE_PERI_REG(I2S_SAMPLE_RATE_CONF_REG(0), (32<<I2S_TX_BITS_MOD_S ) | ( 2<<I2S_TX_BCK_DIV_NUM_S)  | (32<<I2S_RX_BITS_MOD_S) | (2<<I2S_RX_BCK_DIV_NUM_S) );  

#define APLL
#ifdef APLL
	local_rtc_clk_apll_enable( 1, 0, 0, 8, 0 );  // Make internal PLL 640MHz. /4 = 70MHz.

	WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (1<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (2<<I2S_CLKM_DIV_NUM_S) );
	WRITE_PERI_REG( I2S_FIFO_CONF_REG(0), (32<<I2S_RX_DATA_NUM_S) | (32<<I2S_TX_DATA_NUM_S) );
	WRITE_PERI_REG( I2S_CONF_CHAN_REG(0), (1<<I2S_TX_CHAN_MOD_S) | (1<<I2S_RX_CHAN_MOD_S) );
#else
	// fI2S = fCLK / ( N + B/A )
	// DIV_NUM = N
	// Note I2S_CLKM_DIV_NUM minimum = 2 by datasheet.  Less than that and it will ignoreeee you.
	WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (2<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (0<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (1<<I2S_CLKM_DIV_NUM_S) );  // Minimum reduction, 2:1
	WRITE_PERI_REG( I2S_FIFO_CONF_REG(0), (32<<I2S_RX_DATA_NUM_S) | (32<<I2S_TX_DATA_NUM_S) );
	WRITE_PERI_REG( I2S_CONF_CHAN_REG(0), (1<<I2S_TX_CHAN_MOD_S) | (1<<I2S_RX_CHAN_MOD_S) );

#endif

	SET_PERI_REG_BITS(I2S_CONF1_REG(0), I2S_TX_STOP_EN_V, 1, I2S_TX_STOP_EN_S);
	SET_PERI_REG_BITS(I2S_CONF1_REG(0), I2S_TX_PCM_BYPASS_V, 1, I2S_TX_PCM_BYPASS_S);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_TX_RIGHT_FIRST_V, 1, I2S_TX_RIGHT_FIRST_S);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_RX_RIGHT_FIRST_V, 1, I2S_RX_RIGHT_FIRST_S);
	WRITE_PERI_REG( I2S_TIMING_REG(0), 0 );

	// Start returning data to the application
	esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, i2s_isr_fast, (void*)REG_I2S_BASE(0), &i2s_intr_handle);
	esp_intr_enable(i2s_intr_handle);

	isr_countOut = 0;
	i2s_running = true;
	i2s_fill_buf_fast(0);

}


void sandbox_tick()
{
	uprintf( "%d\n", isr_countOut );
	vTaskDelay( 100 );
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnAdvancedUSB = NULL
};
