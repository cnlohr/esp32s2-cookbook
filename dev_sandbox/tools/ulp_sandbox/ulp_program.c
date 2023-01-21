#include <stdint.h>
#include "soc/rtc_cntl_reg.h"
//#include "ulp_riscv_utils.h"

volatile int ulp_global_counter = 0;

void __attribute__((naked, section (".text.vectors"))) reset_vector() 
{
	asm volatile ( "j startup" );
	asm volatile ( "j irq" );
}

void __attribute__((naked)) startup()
{
	asm volatile ( "la sp, __stack_top" );
	asm volatile ( "j main" );
}

void irq()
{
	ulp_global_counter = 0x6666;
}

void __attribute__((noreturn)) main()
{
	//ulp_riscv_rescue_from_monitor(); from ulp_riscv_utils.c
	// Rescue RISCV from monitor state.
	//CLEAR_PERI_REG_MASK(RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_DONE | RTC_CNTL_COCPU_SHUT_RESET_EN);
	//ulp_riscv_rescue_from_monitor();

	int i;
	while( 1 )
	{
		ulp_global_counter = i++;
	}
	//*((uint32_t*)0x00000090) = 0xaaaaaaaa;
}

