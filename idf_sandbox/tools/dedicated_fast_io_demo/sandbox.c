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
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/dedic_gpio_reg.h"
#include "soc/dport_access.h"
#include "soc/gpio_sig_map.h"

extern void * ulp_binary_image;
extern void * ulp_binary_image_end;

int last_ulp_counter;
extern volatile int ulp_global_counter;

int global_i = 100;

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t test_function( uint32_t x );
uint32_t asm_read_gpio();

void sandbox_main()
{
	esp_err_t e;


	DPORT_SET_PERI_REG_MASK( DPORT_CPU_PERI_CLK_EN_REG, DPORT_CLK_EN_DEDICATED_GPIO );
	DPORT_CLEAR_PERI_REG_MASK( DPORT_CPU_PERI_RST_EN_REG, DPORT_RST_EN_DEDICATED_GPIO);

	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<6 );
	REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<6 );
	REG_WRITE( IO_MUX_GPIO6_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC6_OUT_SEL_CFG_REG, PRO_ALONEGPIO_OUT0_IDX );
	REG_WRITE( DEDIC_GPIO_OUT_CPU_REG, 0x01 ); // Enable CPU instruction output
}

void sandbox_tick()
{

	int i;

	for( i = 0; i < 100; i++ )
	{
		// Create a nice little pulse train at 1/10th 
		__asm__ __volatile__ ("set_bit_gpio_out 0x1");
		__asm__ __volatile__ ("nop\nnop\nnop\nnop");
		__asm__ __volatile__ ("clr_bit_gpio_out 0x1");
		// Loop takes 4 nop's.
	} 
}

struct SandboxStruct sandbox_mode =
{
	.idleFunction = sandbox_tick,
	.fnAdvancedUSB = NULL
};
