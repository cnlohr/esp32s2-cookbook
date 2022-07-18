/*
  This abuses the IDF's RMT driver to do ultra sensitive touch/capacitive environment detection within the ESP32-S2.
  */

/*
//	printf( "%08x\n", advanced_usb_scratch_buffer_inst[0] );
	printf( "%p\n", &I2S0 );
	printf( "%p\n",& ledc_timer_config );
	printf( "%p\n",&ledc_channel_config );
	printf( "%p\n",&putchar );
	printf( "%p\n",&rmt_get_ringbuf_handle );
	printf( "%p\n",&rmt_set_source_clk );
	printf( "%p\n",&rmt_set_clk_div );
	printf( "%p\n",&rmt_set_source_clk );
	printf( "%p\n",&rmt_rx_start );
	printf( "%p\n",&rmt_driver_uninstall );	
	printf( "%p\n",&rmt_set_rx_filter );	
	printf( "%p\n",&gpio_isr_handler_remove );
	
	
	
extern void * I2S0;
extern void ledc_timer_config();
extern void ledc_channel_config();

	*/
	
#include <stdio.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/periph_ctrl.h"
#include "esp_err.h"
#include "esp_intr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/lldesc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int global_i = 100;


RingbufHandle_t rb = NULL;
int data0_pin=  17;


static inline uint32_t ccount(void) {
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
  return ccount;
}

int marker1;
int marker0;
int marker;

void gpio_isr_handler( void * v )
{
	int cc = ccount();
	if( REG_READ( GPIO_IN_REG ) & (1<<16 ) )
	{
		marker1 = cc;
	}
	else
	{
		marker0 = cc;
	}
	marker++;
}


static rmt_channel_t example_rx_channel = RMT_CHANNEL_2;


void sandbox_main(display_t * disp)
{
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );

	{
		gpio_config_t cfg = {
		    .pin_bit_mask = BIT64(16),
		    .mode =  GPIO_MODE_INPUT_OUTPUT,
		    .pull_up_en = false,
		    .pull_down_en = false,
		    .intr_type = GPIO_INTR_ANYEDGE,
		};
		gpio_config(&cfg);
		REG_WRITE( GPIO_OUT_W1TC_REG, 1<<16 );
		//REG_WRITE( GPIO_OUT_W1TS_REG, 1<<16 );
	}
	if( 1 ) {
	
		gpio_config_t cfg = {
		    .pin_bit_mask = BIT64(15),
		    .mode =  GPIO_MODE_INPUT_OUTPUT,
		    .pull_up_en = false,
		    .pull_down_en = false,
		    .intr_type = GPIO_INTR_ANYEDGE,
		};
		gpio_config(&cfg);
		REG_WRITE( GPIO_OUT_W1TS_REG, 1<<15 );
		REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<15 );
	}
	
	
	
	gpio_config_t conf0 =
	{
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};

	// Input the data GPIO pins to the I2S module
	conf0.pin_bit_mask = 1LL << data0_pin;
	gpio_config( &conf0 );
	gpio_matrix_in( data0_pin, RMT_SIG_IN0_IDX, false );

#if DO_INTERRUPTS
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(16, gpio_isr_handler, (void*)0);

#else
	rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(data0_pin, example_rx_channel);
	rmt_config(&rmt_rx_config);
	rmt_driver_install( example_rx_channel, 1000, 0 );
	rmt_get_ringbuf_handle( example_rx_channel, &rb );
	rmt_set_clk_div( example_rx_channel, 1 );
	rmt_set_rx_filter( example_rx_channel, 0, 0 );
	rmt_set_source_clk( example_rx_channel, 1 );
	rmt_rx_start(example_rx_channel, true);
	printf( "RB: %p\n", rb );
	
#endif
    
}

void sandbox_exit()
{
#if DO_INTERRUPTS
	gpio_isr_handler_remove( 16 );
#endif
	rmt_driver_uninstall( example_rx_channel );
	ESP_LOGI( "sandbox", "Exit" );
	//esp_intr_disable(i2s_intr_handle);
}

void sandbox_tick()
{
while(1)
{
	int i;
	int j;
	for( j = 0; j < 50; j++ )
	{
		for( i = 0; i < 100; i++ )
			REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<16 );
		for( i = 0; i < 100; i++ )
			REG_WRITE( GPIO_ENABLE_W1TC_REG, 1<<16 );
	}
	
	int tt = 0;
#if !DO_INTERRUPTS
	size_t rx_size = 0;
	rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, 0);
	if(item)
	{
		for (int i = 0; i < rx_size>>2; i++)
		{
			//printf("%d:%dus %d:%dus\n", (item+i)->level0, (item+i)->duration0, (item+i)->level1, (item+i)->duration1);
			if( i != 0 ) tt += (item+i)->duration0;
		}
		vRingbufferReturnItem(rb, (void*) item);
	}
#endif
	ESP_LOGI( "sandbox", "global_i: %d %08x %d,%d", global_i++, REG_READ( GPIO_IN_REG ), marker1 - marker0, tt );
	taskYIELD();
}
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
