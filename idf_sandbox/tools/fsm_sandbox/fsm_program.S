#include <stdint.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_struct.h"
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
	// Rescue RISCV from monitor state.  NOTE: This is not needed if the host does it for us.
	//CLEAR_PERI_REG_MASK(RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_DONE | RTC_CNTL_COCPU_SHUT_RESET_EN);
	//ulp_riscv_rescue_from_monitor();

	RTCIO.enable_w1ts.w1ts = (1<<6);

	volatile uint32_t * w1ts = &RTCIO.out_w1ts.val;
	volatile uint32_t * w1tc = &RTCIO.out_w1tc.val;
	volatile uint32_t * out = &RTCIO.out.val;


	uint32_t gpioval = 1<<(6+10);
	int i;

	// Just FYI - the number of cycles each sw takes is between 7 to 9 depending on alignment.
	asm volatile( "nop" );
	asm volatile( "nop" );
	asm volatile( "nop" );
	while( 1 )
	{
		*w1ts = gpioval;
		*out = 0;
		ulp_global_counter = i++;
	}
	//*((uint32_t*)0x00000090) = 0xaaaaaaaa;
}

