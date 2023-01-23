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
#include "driver/rtc_io.h"
#include "rom/rtc.h"

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

	// Select:
	//	RTC_CNTL_FAST_CLK_RTC_SEL = 
	uint32_t temp = REG_READ( RTC_CNTL_CLK_CONF_REG );
	uprintf( "Original RTC_CNTL_CLK_CONF_REG = %08x\n", temp );

	// Select slow-clock source.  0=RC

	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_ANA_CLK_RTC_SEL_V, 0, RTC_CNTL_ANA_CLK_RTC_SEL_S );

	//RC_FAST_CLK (1) @ ~5-10MHz or XTAL_DIV_CLK (0) @ 10MHz exactly.
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL_V, 0, RTC_CNTL_FAST_CLK_RTC_SEL_S );

	//Set clock divisor.  0 seems to be fastest.
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL_V, 7, RTC_CNTL_CK8M_DIV_SEL_S );

	//Activate divisor.
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL_VLD_V, 1, RTC_CNTL_CK8M_DIV_SEL_VLD_S );

	uint32_t trim = ( ( temp >> 17 ) & 0xff );
	uprintf( "Original trim = %d\n", trim );
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, 255, 255 /* Set trim to 255 */, 17 );

	// Other (reserved) fields don't seem to do anything.

//	CLEAR_PERI_REG_MASK( SYSCON_SYSCLK_CONF_REG, DPORT_PRE_DIV_CNT );

	// RTC_CNTL_SLOW_CLK_CONF_REG seems to have no perf effect.
	CLEAR_PERI_REG_MASK( RTC_CNTL_SLOW_CLK_CONF_REG, RTC_CNTL_ANA_CLK_DIV_VLD );
	SET_PERI_REG_BITS( RTC_CNTL_SLOW_CLK_CONF_REG, RTC_CNTL_ANA_CLK_DIV_V, 0, RTC_CNTL_ANA_CLK_DIV_S );
	SET_PERI_REG_MASK( RTC_CNTL_SLOW_CLK_CONF_REG, RTC_CNTL_ANA_CLK_DIV_VLD );

	rtc_gpio_init( GPIO_NUM_6 );
	rtc_gpio_init( GPIO_NUM_18 );

    rtc_gpio_set_direction(GPIO_NUM_6, RTC_GPIO_MODE_DISABLED);

	ulp_riscv_halt();
	uprintf( "sandbox_main()\n" );
	uint8_t * ulp_begin = (uint8_t*)&ulp_binary_image;
	int ulp_len = (uint8_t*)&ulp_binary_image_end - ulp_begin;
	uprintf( "ULP Image: %p %d\n", ulp_begin, ulp_len );
	e = ulp_riscv_load_binary( ulp_begin, ulp_len );
	uprintf( "Result: %d\n", e );
	ulp_riscv_run(); 

	// Run this to force operation in case it got stuck before boot.
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
