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
#include "soc/rtc.h"
#include "freertos/portmacro.h"

// For clock output
#include "esp_system.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/i2s_reg.h"
#include "soc/periph_defs.h"
#include "rom/lldesc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "soc/regi2c_apll.h"
#include "hal/regi2c_ctrl_ll.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/regi2c_ctrl.h"
#include "hal/clk_tree_ll.h"
#include "pindefs.h"

#define DisableISR()            do { XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL); portbenchmarkINTERRUPT_DISABLE(); } while (0)
#define EnableISR()             do { portbenchmarkINTERRUPT_RESTORE(0); XTOS_SET_INTLEVEL(0); } while (0)

uint32_t pinmaskpower;
uint8_t retbuff[264];
int retisready = 0;

#include "funprog_config.h"
#include "funprog_port.h"
#include "../ch32fun/minichlink/funprog/funprog_rvswdio.h"
#include "bitbang_rvswdio_esp32s2.h"

#if CH5xx_SUPPORT
#include "../ch32fun/minichlink/funprog/funprog_ch5xx.h"
#endif

#if UPDI_SUPPORT
#include "../ch32fun/minichlink/funprog/funprog_updi.h"
#include "bitbang_updi_esp32s2.h"
#endif

#include "../ch32fun/minichlink/funprog/funprog_logic.h"

void sandbox_main()
{
	REG_WRITE( IO_MUX_REG(SWIO_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.  Optional: 10k pull-up resistor. This is the actual SWIO.
	REG_WRITE( IO_MUX_REG(SWCLK_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.  Optional: 10k pull-up resistor. This is the actual SWCLK.
	REG_WRITE( IO_MUX_REG(VDD3V3_EN_PIN), 1<<FUN_IE_S | 1<<FUN_PD_S | 3<<FUN_DRV_S );  //VCC for part 40mA drive.
	REG_WRITE( IO_MUX_REG(VDD5V_EN_PIN), 1<<FUN_IE_S | 1<<FUN_PD_S | 3<<FUN_DRV_S );  //5V for part 40mA drive.

	REG_WRITE( IO_MUX_REG(SWIO_PU_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //SWPUC
	REG_WRITE( IO_MUX_REG(SWCLK_PU_PIN), 1<<FUN_IE_S | 1<<FUN_PU_S | 1<<FUN_DRV_S );  //SWPUC
	GPIO.out_w1ts = 1<<SWIO_PU_PIN;
	GPIO.enable_w1ts = 1<<SWIO_PU_PIN;
	GPIO.out_w1ts = 1<<SWCLK_PU_PIN;
	GPIO.enable_w1ts = 1<<SWCLK_PU_PIN;

	memset( &state, 0, sizeof( state ) );
	memset( &updi, 0, sizeof( updi ) );
	state.pinmaskD = 1<<SWIO_PIN;
	state.pinmaskC = 1<<SWCLK_PIN;

	pinmaskpower = (1<<VDD3V3_EN_PIN) | (1<<VDD5V_EN_PIN);
	GPIO.out_w1ts = pinmaskpower;
	GPIO.enable_w1ts = pinmaskpower;

	rtc_cpu_freq_config_t m;
	rtc_clk_cpu_freq_get_config( &m );

#if defined(UPDI_SUPPORT) && UPDI_SUPPORT
	updi.pinmask = ( 1<<SWIO_PIN );
	updi.pinmaskpower = pinmaskpower;
	GPIO.out_w1ts = state.pinmaskD;
	GPIO.enable_w1ts = state.pinmaskD;
	updi.clocks_per_bit = UPDIComputeClocksPerBit( m.freq_mhz, 115200 );
#endif

	esp_rom_delay_us(5000);

	switch( m.freq_mhz )
	{
	case 240:
		state.t1coeff = 10; // 9 or 10 is good.  5 is too low. 13 is sometimes too high.
		break;
	default:
		state.t1coeff = 100; // Untested At Other Speeds
		break;
	}
}

void teardown()
{
	// Power-Down
	GPIO.out_w1tc = 1<<SWIO_PIN;
	GPIO.out_w1ts = 1<<SWIO_PIN;
	GPIO.out_w1tc = 1<<VDD3V3_EN_PIN;
	GPIO.out_w1tc = 1<<VDD5V_EN_PIN;	
	GPIO.out_w1tc = 1<<SWIO_PU_PIN;
}

void sandbox_tick()
{
	//esp_rom_delay_us(100);
}

int16_t funprog_usb_feature_report( uint8_t * buffer, uint16_t reqlen, uint8_t is_get )
{
	if( is_get )
	{
		if( !retisready ) { buffer[0] = 0xff; return reqlen; }
		retisready = 0;
		int len = retbuff[0];
		FUN_PRINTF_DEBUG( "IRQORET: %d\n", len );
		if( len > reqlen-1 ) len = reqlen-1;
		memcpy( buffer, retbuff, len+1 );
		return reqlen;
	}
	
	// Is send.
	// buffer[0] is the request ID.
	HandleCommandBuffer( buffer, reqlen, retbuff, sizeof(retbuff) - 1 );
	retisready = 1;
	return 0;
}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnDecom = teardown,
	.fnAdvancedUSB = funprog_usb_feature_report,
};
