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
#include "ulp_riscv.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/system_reg.h"
#include "soc/syscon_reg.h"

extern void * ulp_binary_image;
extern void * ulp_binary_image_end;

int last_ulp_counter;
extern volatile int ulp_global_counter;

int global_i = 100;

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t test_function( uint32_t x );
uint32_t asm_read_gpio();

static inline uint32_t getCycleCount()
{
    uint32_t ccount;
    asm volatile("rsr %0,ccount":"=a" (ccount));
    return ccount;
}

void sandbox_main()
{
	esp_err_t e;

	// Select RC_FAST_CLK
	REG_WRITE( RTC_CNTL_CLK_CONF_REG, (2<<RTC_CNTL_ANA_CLK_RTC_SEL_S) | (0<<RTC_CNTL_FAST_CLK_RTC_SEL_S) );
	
	CLEAR_PERI_REG_MASK( SYSCON_SYSCLK_CONF_REG, DPORT_PRE_DIV_CNT );
	ulp_riscv_halt();
	uprintf( "sandbox_main()\n" );
	uint8_t * ulp_begin = (uint8_t*)&ulp_binary_image;
	int ulp_len = (uint8_t*)&ulp_binary_image_end - ulp_begin;
	uprintf( "ULP Image: %p %d\n", ulp_begin, ulp_len );
	e = ulp_riscv_load_binary( ulp_begin, ulp_len );
	uprintf( "Result: %d\n", e );
	ulp_riscv_run(); 

	// Not sure why we need to run this.
	CLEAR_PERI_REG_MASK( RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_DONE );
}

void sandbox_tick()
{
	//uint32_t start = getCycleCount();
	//uint32_t end = getCycleCount();
	
	uint32_t counter = ulp_global_counter;
	uint32_t delta = counter - last_ulp_counter;
	last_ulp_counter = counter;
	uprintf( "ULP: %d\n", delta ); 
	
	vTaskDelay( 100 );
	SET_PERI_REG_MASK( RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_SW_INT_TRIGGER );
}

struct SandboxStruct sandbox_mode =
{
	.idleFunction = sandbox_tick,
	.fnAdvancedUSB = NULL
};
