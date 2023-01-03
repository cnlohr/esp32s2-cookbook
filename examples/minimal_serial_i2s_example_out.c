/* USB console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include "driver/uart.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/i2s_reg.h"
#include "soc/periph_defs.h"
#include "rom/lldesc.h"
#include "private_include/regi2c_apll.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "regi2c_ctrl.h"
#include "driver/periph_ctrl.h"

#define DMX_UART UART_NUM_1

// On the ESP32S2 (TRM, Page 288)
#define ETS_I2S0_INUM 35

#define BUFF_SIZE_BYTES 2048


#define MAX_DMX 1024

uint32_t * i2sbuffer[4] __attribute__((aligned(128)));
uint8_t dmx_buffer[1024 + 4] = { 0 };
volatile int highest_send = 0;
static intr_handle_t i2s_intr_handle;


int isr_countOut;
int i2s_running;

static void IRAM_ATTR i2s_isr_fast(void* arg) {
	REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
	++isr_countOut;
}


uint16_t tud_hid_get_report_cb(uint8_t itf,
							   uint8_t report_id,
							   hid_report_type_t report_type,
							   uint8_t* buffer,
							   uint16_t reqlen)
{
	if( report_id == 0xad )
	{
		// DMX packet
		// No special packet supported.
	}
	else
	{
		printf( "Custom Report Get %d %d %02x [%d]\n", itf, report_id, report_type, reqlen );
	}
	return 0;
}


void tud_hid_set_report_cb(uint8_t itf,
						   uint8_t report_id,
						   hid_report_type_t report_type,
						   uint8_t const* buffer,
						   uint16_t bufsize )
{
	// Code 0xad has a special setting in TUD_HID_REPORT_DESC_GAMEPAD to allow unrestricted HIDAPI access from Windows.
	if( report_id == 0xad && buffer[1] == 0x73 )
	{
		// DMX packet
		int offset = buffer[2] * 4;
		int towrite = buffer[3];
		if( offset + towrite > sizeof( dmx_buffer ) )
			towrite = sizeof( dmx_buffer ) - offset;
		memcpy( dmx_buffer + offset + 4, buffer + 4, towrite );

		if( offset + towrite > highest_send ) highest_send = offset + towrite;

		//printf( "%02x %02x %02x %02x %d %d %d %d %d\n", buffer[0], buffer[1], buffer[2], buffer[3], highest_send, offset, bufsize, buffer[bufsize], towrite );

		if( offset == 0 )
		{
			// Actually send.
			// XXX TODO
		}
	}
	else
	{
		printf( "Custom Report Set %d %d[%02x] %02x [%d]\n", itf, report_id, buffer[0], report_type, bufsize );
	}
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
		memset( i2sbuffer[i], 0xf0, BUFF_SIZE_BYTES );
	}
	return ESP_OK;
}



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

void app_main(void)
{
	printf( "Starting\n" );
	fflush(stdout);



//	intr_matrix_set(0, ETS_I2S0_INTR_SOURCE, ETS_I2S0_INUM);

	dma_desc_init();

	gpio_num_t pins[] = {
		GPIO_NUM_17,
		GPIO_NUM_18,
	};

	gpio_config_t conf = {
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	for (int i = 0; i < sizeof(pins)/sizeof(gpio_num_t); ++i) {
		conf.pin_bit_mask = 1LL << pins[i];
		gpio_config(&conf);
	}

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

	// fI2S = fCLK / ( N + B/A )
	// For 250kBAUD, N=40 (because of other /2's) if selecting 160M clock.
	// DIV_NUM = N
	// Note I2S_CLKM_DIV_NUM minimum = 2.
	WRITE_PERI_REG( I2S_CLKM_CONF_REG(0), (2<<I2S_CLK_SEL_S) | (1<<I2S_CLK_EN_S) | (1<<I2S_CLKM_DIV_A_S) | (0<<I2S_CLKM_DIV_B_S) | (40<<I2S_CLKM_DIV_NUM_S) );  // Minimum reduction, 2:1
	WRITE_PERI_REG( I2S_FIFO_CONF_REG(0), (32<<I2S_RX_DATA_NUM_S) | (32<<I2S_TX_DATA_NUM_S) );
	WRITE_PERI_REG( I2S_CONF_CHAN_REG(0), (1<<I2S_TX_CHAN_MOD_S) | (1<<I2S_RX_CHAN_MOD_S) );

	SET_PERI_REG_BITS(I2S_CONF1_REG(0), I2S_TX_STOP_EN_V, 1, I2S_TX_STOP_EN_S);
	SET_PERI_REG_BITS(I2S_CONF1_REG(0), I2S_TX_PCM_BYPASS_V, 1, I2S_TX_PCM_BYPASS_S);

	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_TX_RIGHT_FIRST_V, 1, I2S_TX_RIGHT_FIRST_S);
	SET_PERI_REG_BITS(I2S_CONF_REG(0), I2S_RX_RIGHT_FIRST_V, 1, I2S_RX_RIGHT_FIRST_S);

	WRITE_PERI_REG( I2S_TIMING_REG(0), 0 );

	// Start returning data to the application
	esp_intr_alloc(ETS_I2S0_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, i2s_isr_fast, (void*)REG_I2S_BASE(0), &i2s_intr_handle);
	esp_intr_enable(i2s_intr_handle);

	// Maximize the drive strength.
	gpio_set_drive_capability( GPIO_NUM_17, GPIO_DRIVE_CAP_3 );
	gpio_set_drive_capability( GPIO_NUM_18, GPIO_DRIVE_CAP_3 );

	isr_countOut = 0;
	i2s_running = true;
	i2s_fill_buf_fast(0);

	/* Initialize USB peripheral */
	tinyusb_config_t tusb_cfg = {};
	tinyusb_driver_install(&tusb_cfg);

//	vTaskSuspend( 0 );

//	uart_write_bytes_with_break(DMX_UART, "", 1, 20 ); // Can't use uart_write_bytes_with_break because break needs to go right before frame.

	/* Main loop */
	while(true)
	{
		printf( "%d\n", isr_countOut);
		vTaskDelay( 100 );
	}
	return;
}
