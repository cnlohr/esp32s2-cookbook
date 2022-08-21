#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "swadgeMode.h"
#include "hdw-led/led_util.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"
#include "soc/ledc_reg.h"
#include "soc/system_reg.h"

#include "meleeMenu.h"

int global_i = 100;
meleeMenu_t * menu;
font_t meleeMenuFont;
display_t * disp;

//#define REBOOT_TEST
//#define PROFILE_TEST
#define REG_LEDC_TEST


// Example to do true inline assembly.  This will actually compile down to be
// included in the code, itself, and "should" (does in all the tests I've run)
// execute in one clock cycle since there is no function call and rsr only
// takes one cycle to complete. 
static inline uint32_t get_ccount()
{
	uint32_t ccount;
	asm volatile("rsr %0,ccount":"=a" (ccount));
	return ccount;
}

// External functions defined in .S file for you assembly people.
void minimal_function();
uint32_t test_function( uint32_t x );
uint32_t asm_read_gpio();

void menuCb(const char* opt)
{
}

void sandbox_main(display_t * disp_in)
{
//	memset( &meleeMenuFont, 0, sizeof( meleeMenuFont ) );
	menu = 0; disp = disp_in;
	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );

	REG_WRITE( GPIO_FUNC7_OUT_SEL_CFG_REG,4 ); // select ledc_ls_sig_out0

#ifdef REBOOT_TEST
	// Uncomment this to reboot the chip into the bootloader.
	// This is to test to make sure we can call ROM functions.
	REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
	void software_reset( uint32_t x );
	software_reset( 0 );
#endif

#ifdef REG_LEDC_TEST
	// Valid things are :
	// REG_WRITE, REG_SET_BIT, REG_CLR_BIT and REG_SET_FIELD
	// Note if you want the field offset, you need to add a _S
	// to field names.

#define LED_TFTATN 33
#define LED_TFTATP 35

/*
	REG_WRITE( GPIO_OUT_W1TC_REG, 1<<(7) );
	REG_WRITE( GPIO_ENABLE_W1TS_REG, 1<<(7) );
	REG_WRITE( IO_MUX_GPIO7_REG, 0 );
*/
	// Disable power going to the + side of the backlight fet.
	REG_WRITE( GPIO_OUT1_W1TS_REG, 1<<(LED_TFTATP-32) );

	// Enable GPIO7 Output, but start by driving it low, to be safe.
	REG_WRITE( GPIO_OUT1_W1TC_REG, 1<<(LED_TFTATN-32) );
	REG_WRITE( GPIO_ENABLE1_W1TS_REG, 1<<(LED_TFTATN-32) );

	REG_WRITE( GPIO_OUT1_W1TS_REG, 1<<(LED_TFTATN-32) );
	REG_WRITE( GPIO_OUT1_W1TC_REG, 1<<(LED_TFTATN-32) );

	REG_WRITE( GPIO_PIN33_REG, 0 );
	REG_WRITE( IO_MUX_GPIO33_REG, 2<<FUN_DRV_S );
	REG_WRITE( GPIO_FUNC33_OUT_SEL_CFG_REG, 79 ); // select ledc_ls_sig_out0

//	REG_SET_BIT( DPORT_PERIP_RST_EN0_REG, DPORT_LEDC_RST );
	REG_CLR_BIT( DPORT_PERIP_RST_EN0_REG, DPORT_LEDC_RST );
	REG_SET_BIT( DPORT_PERIP_CLK_EN_REG, DPORT_LEDC_CLK_EN );

	REG_WRITE( LEDC_CONF_REG,
		1 << LEDC_APB_CLK_SEL_S | // Configure LEDC_PWM_CLK to be APB_CLK
		1 << LEDC_CLK_EN_S );

	REG_WRITE( LEDC_LSTIMER0_CONF_REG, 
		1 << LEDC_LSTIMER0_PARA_UP_S |
		0x150 << LEDC_CLK_DIV_LSTIMER0_S | // MSB = A, LSB = B/256 DIV = A + B/256
		0 << LEDC_TICK_SEL_LSTIMER0_S |  // Use clock timer (0), not systick (1).
		8 << LEDC_LSTIMER0_DUTY_RES_S ); // 8 bit pre-timer (0..255)

	REG_WRITE( LEDC_LSCH0_CONF0_REG,
		0 << LEDC_TIMER_SEL_LSCH0_S | 	// Configure this channel to use timer0
		0 << LEDC_OVF_CNT_EN_LSCH0_S |
		0 << LEDC_OVF_NUM_LSCH0_S |
		0 << LEDC_SIG_OUT_EN_LSCH0_S |
		1 << LEDC_PARA_UP_LSCH0_S |
		1 << LEDC_IDLE_LV_LSCH0_S       // Default low level.
	);

	// LED Frequency is deterined by:
	//  fLED = APB_BUS_CLOCK / ( 2^LEDC_LSTIMER0_DUTY_RES_S * LEDC_CLK_DIV_LSTIMER0_S )

	REG_WRITE( LEDC_LSCH0_HPOINT_REG, 0 );
	REG_WRITE( LEDC_LSCH0_DUTY_REG, 100 );  // Clever this dithers, the bottom 2 bits are enacted every nth cycle.

	REG_SET_BIT( LEDC_LSCH0_CONF0_REG, LEDC_SIG_OUT_EN_LSCH0 );
	REG_SET_BIT( LEDC_LSCH0_CONF0_REG, LEDC_PARA_UP_LSCH0 ); // Update HPOINT, DUTY, and turn on.

	// No duty stepping, but we have to set it to start otherwise, we only
	// get a one-shot PWM firing.
	REG_WRITE( LEDC_LSCH0_CONF1_REG, (1<<LEDC_DUTY_START_LSCH0_S) );

	// Now, enable 3.3V on the inductor.
	REG_WRITE( GPIO_OUT1_W1TC_REG, 1<<(LED_TFTATP-32) );
	REG_WRITE( GPIO_ENABLE1_W1TS_REG, 1<<(LED_TFTATP-32) );

	loadFont("mm.font", &meleeMenuFont);
	menu = initMeleeMenu("Hello", &meleeMenuFont, menuCb);
	addRowToMeleeMenu(menu, "Test 1");
	addRowToMeleeMenu(menu, "Test 2");
	addRowToMeleeMenu(menu, "Test 3");
#endif

	ESP_LOGI( "sandbox", "Loaded" );
}

void sandbox_exit()
{
	ESP_LOGI( "sandbox", "Exit" );
	if( menu )
	{
		deinitMeleeMenu(menu);
		freeFont(&meleeMenuFont);
	}

	ESP_LOGI( "sandbox", "Exit" );
}

void sandbox_tick()
{
#ifdef PROFILE_TEST
	volatile uint32_t profiles[7];  // Use of volatile here to force compiler to order instructions and not cheat.
	uint32_t start, end;

	// Profile function call into assembly land
	// Mostly used to understand function call overhead.
	start = get_ccount();
	minimal_function();
	end = get_ccount();
	profiles[0] = end-start-1;

	// Profile a nop (Should be 1, because profiling takes 1 cycle)
	start = get_ccount();
	asm volatile( "nop" );
	end = get_ccount();
	profiles[1] = end-start-1;

	// Profile reading a register (will be slow)
	start = get_ccount();
	READ_PERI_REG( GPIO_ENABLE_W1TS_REG );
	end = get_ccount();
	profiles[2] = end-start-1;

	// Profile writing a regsiter (will be fast)
	// The ESP32-S2 can "write" to memory and keep executing
	start = get_ccount();
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	end = get_ccount();
	profiles[3] = end-start-1;

	// Profile subsequent writes (will be slow)
	// The ESP32-S2 can only write once in a buffered write.
	start = get_ccount();
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	WRITE_PERI_REG( GPIO_ENABLE_W1TS_REG, 0 );
	end = get_ccount();
	profiles[4] = end-start-1;

	// Profile a more interesting assembly instruction
	start = get_ccount();
	uint32_t tfret = test_function( 0xaaaa );
	end = get_ccount();
	profiles[5] = end-start-1;

	// Profile a more interesting assembly instruction
	start = get_ccount();
	uint32_t tfret2 = asm_read_gpio( );
	end = get_ccount();
	profiles[6] = end-start-1;

	vTaskDelay(1);

	ESP_LOGI( "sandbox", "global_i: %d %d %d %d %d %d %d clock cycles; tf ret: %08x / %08x", profiles[0], profiles[1], profiles[2], profiles[3], profiles[4], profiles[5], profiles[6], tfret, tfret2 );
#elif defined( REG_LEDC_TEST )
//	ESP_LOGI( "sandbox", "global_i: %d %08x %3d / %08x", global_i++, REG_READ( LEDC_LSTIMER0_CONF_REG ), REG_READ( LEDC_LSTIMER0_VALUE_REG ), REG_READ( LEDC_LSCH0_CONF0_REG ) ); 	
#else
	ESP_LOGI( "sandbox", "global_i: %d", global_i++ );
#endif

	if( menu )
	    drawMeleeMenu(disp, menu);

}

void sandbox_button(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch (evt->button)
        {
            case UP:
            case DOWN:
            case LEFT:
            case RIGHT:
            case BTN_A:
            {
                meleeMenuButton(menu, evt->button);
                break;
            }
            case START:
            case SELECT:
            case BTN_B:
            default:
            {
                break;
            }
        }
    }
}

swadgeMode sandbox_mode =
{
    .modeName = "sandbox",
    .fnEnterMode = sandbox_main,
    .fnExitMode = sandbox_exit,
    .fnMainLoop = sandbox_tick,
    .fnButtonCallback = sandbox_button,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};
