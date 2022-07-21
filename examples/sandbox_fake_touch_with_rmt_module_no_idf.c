/*
  This abuses the IDF's RMT driver to do ultra sensitive touch/capacitive environment detection within the ESP32-S2.
  
  This assumes that you have a piece of wire or something connected to GPIO16, and a 200k _ish_ resistor between
  that and Vcc (or pin 15 which we just tie high).
  
  Be sure you aren't using the RMT module with IDF stuff.
  
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
#include "freertos/task.h"
#include "rom/lldesc.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "soc/rmt_reg.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "soc/interrupt_reg.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int quit = 0;
int global_i = 0;

static inline uint32_t ccount(void) {
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
  return ccount;
}

int marker = 0;
 
void rmt_intr_handler( void * v )
{
	WRITE_PERI_REG(RMT_INT_CLR_REG,RMT_CH3_RX_END_INT_CLR);
	//int cc = ccount();
	marker++;
}



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
	}
	{
	
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
	
	gpio_matrix_in( 16, RMT_SIG_IN3_IDX, false );

	//INTERRUPT_PRO_RMT_INTR_MAP_REG

	xt_set_interrupt_handler( ETS_RMT_INTR_SOURCE, &rmt_intr_handler, 0 );

	REG_SET_BIT( DPORT_PERIP_RST_EN0_REG, DPORT_RMT_RST );
	REG_CLR_BIT( DPORT_PERIP_RST_EN0_REG, DPORT_RMT_RST );
	REG_SET_BIT( DPORT_PERIP_CLK_EN0_REG, DPORT_RMT_CLK_EN );

	REG_CLR_BIT( RMT_APB_CONF_REG, RMT_MEM_FORCE_PU );
	REG_SET_BIT( RMT_APB_CONF_REG, RMT_MEM_FORCE_PD );
	REG_CLR_BIT( RMT_APB_CONF_REG, RMT_MEM_FORCE_PD );
	REG_SET_BIT( RMT_APB_CONF_REG, RMT_MEM_FORCE_PU );

	WRITE_PERI_REG( RMT_APB_CONF_REG, 
		1<<RMT_APB_FIFO_MASK_S | // 1 = direct access, 0 = fifo. According to docs "We highly recommended to use MEM mode not FIFO mode since there will be some gotcha in FIFO mode."
		1<<RMT_MEM_TX_WRAP_EN_S |
		1<<RMT_MEM_FORCE_PU_S |
		1<<RMT_MEM_CLK_FORCE_ON_S |
		1<<RMT_CLK_EN_S ); 
	
	// Reset appropriate parts.
	WRITE_PERI_REG( RMT_CH3CONF1_REG,
		(1<<RMT_REF_ALWAYS_ON_CH3_S ) | //1 for APB_CLK
		(1<<RMT_APB_MEM_RST_CH3_S) |
		(1<<RMT_MEM_WR_RST_CH3_S) | // For RX 
		(1<<RMT_MEM_RD_RST_CH3_S) ); // For TX

	WRITE_PERI_REG( RMT_CH3CONF1_REG,
		(0<<RMT_APB_MEM_RST_CH3_S) |
		(0<<RMT_MEM_WR_RST_CH3_S) |
		(0<<RMT_MEM_RD_RST_CH3_S) );

	WRITE_PERI_REG( RMT_CH3_RX_CARRIER_RM_REG, 0x00000000 );
	WRITE_PERI_REG( RMT_CH3CARRIER_DUTY_REG, 0x00000000);
	
	WRITE_PERI_REG( RMT_CH3CONF0_REG, 
		(1<<RMT_DIV_CNT_CH3_S) |
		(0x7fff<<RMT_IDLE_THRES_CH3_S) |
		(0<<RMT_CARRIER_EFF_EN_CH3_S ) |
		(0<<RMT_CARRIER_EN_CH3_S) |
		(1<<RMT_MEM_SIZE_CH3_S) );

	WRITE_PERI_REG( RMT_CH3CONF1_REG, 
		(0<<RMT_REF_ALWAYS_ON_CH3_S ) | //1 for APB_CLK  
		(0<<RMT_RX_FILTER_EN_CH3_S ) | 
		(1<<RMT_MEM_OWNER_CH3_S ) | //1 = RX, 0 = TX
		(0<<RMT_RX_FILTER_EN_CH3_S) |
		(0<<RMT_RX_FILTER_THRES_CH3_S) |
		(1<<RMT_REF_ALWAYS_ON_CH3_S ) | //1 for APB_CLK
		(1<<RMT_CHK_RX_CARRIER_EN_CH3_S) ); //XXX TODO

	REG_SET_BIT( RMT_CH3CONF1_REG, RMT_RX_EN_CH3 );

	REG_SET_BIT( RMT_REF_CNT_RST_REG, RMT_REF_CNT_RST_CH3 );
	REG_SET_BIT( RMT_INT_ENA_REG, RMT_CH3_RX_END_INT_ENA );
 
	ESP_LOGI( "sandbox", "Setup. %08x", READ_PERI_REG( DPORT_PRO_RMT_INTR_MAP_REG ) ); 

	//esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle)?
}

void sandbox_exit()
{
	quit = 1;
	REG_CLR_BIT( RMT_APB_CONF_REG, RMT_MEM_FORCE_PU );
	REG_SET_BIT( RMT_APB_CONF_REG, RMT_MEM_FORCE_PD );
	REG_SET_BIT( DPORT_PERIP_RST_EN0_REG, DPORT_RMT_RST );
	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
	while(!quit)
	{
		int i;
		int j;
		REG_SET_BIT( RMT_CH3CONF1_REG, RMT_MEM_WR_RST_CH3 );
		
		portDISABLE_INTERRUPTS();
		for( j = 0; j < 70; j++ )
		{
			for( i = 0; i < 5; i++ )
				REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<16 );
			for( i = 0; i < 60; i++ )
				REG_WRITE( GPIO_ENABLE_W1TC_REG, 1<<16 );
		}
		portENABLE_INTERRUPTS();
		

		uint32_t rsum = 0;
		int isumct = 0;
		uint32_t * r = (uint32_t*)&(RMTMEM);
		REG_CLR_BIT( RMT_CH3CONF1_REG, RMT_MEM_OWNER_CH3 );
		for( i = 0; i < 64; i++ )
		{
			uint32_t rrr = r[i+12*16];
			uint16_t vr = ( rrr & 0x7fff ); 
			if( vr > 10 && vr < 0x1000 && ( rrr & 0x80000000) ) 
			{
				rsum += vr;
				isumct++;
				if( isumct > 50 ) break;
			}
		} 
		REG_SET_BIT( RMT_CH3CONF1_REG, RMT_MEM_OWNER_CH3 );
		ESP_LOGI( "sandbox", ",%d", rsum );
#if 0
 
		//ESP_LOGI( "sandbox", "global_i: %d %d %08x %08x %08x", global_i++, marker, rm[2].val, READ_PERI_REG( RMT_CH3STATUS_REG ), READ_PERI_REG( RMT_CH3ADDR_REG ) );
		int oi = 0;
		for( oi = 12; oi < 16; oi++ )
		{
			uint32_t * r = (uint32_t*)&(RMTMEM);
			char rlog[1024]; 
			char * rr = rlog;
		 REG_CLR_BIT( RMT_CH3CONF1_REG, RMT_MEM_OWNER_CH3 );
			for( i = 0; i < 16; i++ )
			{
				rr += sprintf( rr, " %08x", r[i+oi*16] );
			} 
		 REG_SET_BIT( RMT_CH3CONF1_REG, RMT_MEM_OWNER_CH3 );
			
	//#endif
	//		ESP_LOGI( "sandbox", "%p", &RMTMEM );
			ESP_LOGI( "sandbox", "%08x %08x %04x %s", READ_PERI_REG( RMT_CH3STATUS_REG ), READ_PERI_REG( RMT_CH3ADDR_REG ) , oi*16, rlog );
	 }
 
			#endif

		global_i++;
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
