#include <stdint.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_struct.h"
#include "soc/sens_struct.h"
#include "soc/rtc_cntl_struct.h"
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

//	RTCIO.enable_w1ts.w1ts = (1<<6);
	RTCIO.enable_w1ts.w1ts = (1<<18);

	volatile uint32_t * w1ts = &RTCIO.out_w1ts.val;
	volatile uint32_t * w1tc = &RTCIO.out_w1tc.val;
	volatile uint32_t * out = &RTCIO.out.val;

	SENS.sar_touch_conf.touch_outen = 0xffffff;
	RTCIO.touch_pad[6].xpd = 1; // touch power on.
	RTCIO.touch_pad[6].fun_sel = 0;
	RTCCNTL.pad_hold.touch_pad6_hold = 0;
    RTCCNTL.touch_ctrl2.touch_start_force = 1; // Software force on

	// from touch_ll_stop_fsm
    RTCCNTL.touch_ctrl2.touch_start_en = 0; //stop touch fsm
    RTCCNTL.touch_ctrl2.touch_slp_timer_en = 0;
    RTCCNTL.touch_ctrl2.touch_timer_force_done = 3; //TOUCH_LL_TIMER_FORCE_DONE;
    RTCCNTL.touch_ctrl2.touch_timer_force_done = 0; //TOUCH_LL_TIMER_DONE;

	RTCCNTL.touch_ctrl2.touch_clkgate_en = 1;

	RTCIO.touch_pad[6].start = 1; // touch power on.
	RTCIO.touch_pad[6].rue= 1;

	SENS.sar_touch_chn_st.touch_pad_active = 1<<6;

    RTCCNTL.touch_scan_ctrl.touch_scan_pad_map  |= ((1<<6));
    SENS.sar_touch_conf.touch_outen |= ((1<<6));

	uint32_t gpioval = 0xffff<<(6+10);
	int i;

	// Just FYI - the number of cycles each sw takes is between 7 to 9 depending on alignment.
	asm volatile( "nop" );
	asm volatile( "nop" );
	asm volatile( "nop" );
	while( 1 )
	{
		// This actually triggers it.
		RTCCNTL.touch_ctrl2.touch_start_en = 0;
		RTCCNTL.touch_ctrl2.touch_start_en = 1;
		int i;
		for( i = 0; i < 1000; i++ ) asm volatile( "nop" );
		*w1ts = gpioval;
		*out = 0;
		ulp_global_counter = i++;
	}
	//*((uint32_t*)0x00000090) = 0xaaaaaaaa;
}

