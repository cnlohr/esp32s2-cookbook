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
#include "soc/rtc_cntl_reg.h"
#include "soc/system_reg.h"
#include "soc/syscon_reg.h"
#include "driver/rtc_io.h"
#include "rom/rtc.h"
#include "esp32s2/ulp.h"
#include "ulp_fsm_common.h"
#include "soc/rtc_io_reg.h"


//Yuck - because the sandbox is built for the RISC-V ULP
// we have to do this this to get access to the FSM ULP.
#pragma GCC diagnostic ignored "-Wformat"
#include "ulp/ulp_fsm/ulp_macro.c"
#define TAG NOTAG
#include "ulp/ulp_fsm/ulp.c"


int last_ulp_counter;

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
	// Select:
	//	RTC_CNTL_FAST_CLK_RTC_SEL = 
	uint32_t temp = REG_READ( RTC_CNTL_CLK_CONF_REG );
	uprintf( "Original RTC_CNTL_CLK_CONF_REG = %08x\n", temp );

	// Select slow-clock source.  0=RC
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_ANA_CLK_RTC_SEL_V, 0, RTC_CNTL_ANA_CLK_RTC_SEL_S );

	//RC_FAST_CLK (1) @ ~5-10MHz or XTAL_DIV_CLK (0) @ 10MHz exactly.
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_FAST_CLK_RTC_SEL_V, 1, RTC_CNTL_FAST_CLK_RTC_SEL_S );

	//Set clock divisor.  0 seems to be fastest.
	SET_PERI_REG_BITS( RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL_V, 0, RTC_CNTL_CK8M_DIV_SEL_S );

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
	rtc_gpio_set_direction( GPIO_NUM_6, RTC_GPIO_MODE_OUTPUT_ONLY );

	// Depressing - when using FSM, it looks like it takes
	// 12 cycles to output to a port(!!) :( :(
	// This toggles GPIO6.

	ulp_insn_t program[] = {
		M_LABEL( 1 ),	// Label 1
		I_WR_REG_BIT(RTC_GPIO_OUT_REG, 16, 1),
		I_WR_REG_BIT(RTC_GPIO_OUT_REG, 16, 0),
		M_BX(1),
		I_HALT()
	};


	size_t load_addr = 0;
	size_t size = sizeof(program)/sizeof(ulp_insn_t);
	ulp_process_macros_and_load(load_addr, program, &size);
	ulp_run(load_addr);

	// Run this to force operation in case it got stuck before boot.
//	CLEAR_PERI_REG_MASK( RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_DONE );
}

void sandbox_tick()
{
	//uint32_t start = getCycleCount();
	//uint32_t end = getCycleCount();
	
	uint32_t counter = *((uint32_t*)0x50000010);
	//uint32_t delta = counter - last_ulp_counter;
	last_ulp_counter = counter;
	uprintf( "ULP: %d\n", counter );  
	int i;
	for( i = 0; i < 40; i++ )
	{
		uprintf( "%08x ", *((uint32_t*)(i*4+0x50000000)) );
	}
	uprintf( "\n" );


	vTaskDelay( 100 );
	//SET_PERI_REG_MASK( RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_SW_INT_TRIGGER );
}

struct SandboxStruct sandbox_mode =
{
	.idleFunction = sandbox_tick,
	.fnAdvancedUSB = NULL
};
