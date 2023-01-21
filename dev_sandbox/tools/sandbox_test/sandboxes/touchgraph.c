#include <stdio.h>
#include <string.h>


#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/adc.h"
//#include "esp_adc_cal.h"
#include "esp_log.h"
#include "driver/temp_sensor.h"
#include "esp_check.h"
#include "soc/spi_reg.h"
#include "rom/lldesc.h"
#include "soc/periph_defs.h"
#include "hal/adc_hal_conf.h"
#include "soc/system_reg.h"
#include "hal/gpio_types.h"
#include "soc/system_reg.h"
#include "soc/dport_access.h"
#include "soc/dedic_gpio_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_log.h"
#include "soc/rtc.h"
#include "soc/rtc_io_reg.h"
#include "soc/apb_saradc_reg.h"
#include "soc/sens_reg.h"
#include "hal/adc_ll.h"
#include "driver/rtc_io.h"
#include "soc/spi_reg.h"

#include "hal/touch_sensor_ll.h"


#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"

#include "soc/dedic_gpio_reg.h"

#include "swadgeMode.h"
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"
#include "meleeMenu.h"

volatile int global_i = 100;
meleeMenu_t * menu;
font_t meleeMenuFont;
display_t * disp;
volatile int doexit;

//#define REBOOT_TEST
//#define PROFILE_TEST
#define REG_LEDC_TEST

paletteColor_t * GetPixelBuffer();

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

#define QDELAY { int i; for( i = 0; i < 30; i++ ) __asm__ __volatile__ ("nop\nnop\nnop\nnop\nnop"); }


void menuCb(const char* opt)
{
}

void sandbox_main(display_t * disp_in)
{
	ESP_LOGI( "sandbox", "sandbox_main" );
	doexit = 0;

	loadFont("mm.font", &meleeMenuFont);
	disp = disp_in;
	menu = initMeleeMenu("Hello", &meleeMenuFont, menuCb);
	addRowToMeleeMenu(menu, "Test 1");
	addRowToMeleeMenu(menu, "Test 2");
	addRowToMeleeMenu(menu, "Test 3");

	ESP_LOGI( "sandbox", "Loaded X" );
}

void sandbox_exit()
{
	doexit = 1;

	ESP_LOGI( "sandbox", "Exit" );
	if( menu )
	{
		deinitMeleeMenu(menu);
		freeFont(&meleeMenuFont);
	}

	ESP_LOGI( "sandbox", "Exit" );
}

uint32_t touches[512][6];
int touchhead = 0;

static const int8_t sinlut[320] = { 0, 2, 5, 7, 10, 12, 15, 17, 20, 22, 24, 27, 29, 32, 34, 37, 39, 41, 44, 46, 48, 51, 53, 55, 58, 60, 62, 64, 66, 69, 71, 73, 75, 77, 79, 81, 83, 85, 86, 88, 90, 92, 93, 95, 97, 98, 100, 102, 103, 105, 106, 107, 109, 110, 111, 112, 114, 115, 116, 117, 118, 119, 120, 120, 121, 122, 123, 123, 124, 125, 125, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 126, 126, 126, 125, 125, 124, 123, 123, 122, 121, 120, 120, 119, 118, 117, 116, 115, 114, 112, 111, 110, 109, 107, 106, 105, 103, 102, 100, 98, 97, 95, 93, 92, 90, 88, 86, 85, 83, 81, 79, 77, 75, 73, 71, 69, 66, 64, 62, 60, 58, 55, 53, 51, 48, 46, 44, 41, 39, 37, 34, 32, 29, 27, 24, 22, 20, 17, 15, 12, 10, 7, 5, 2, 0, -2, -5, -7, -10, -12, -15, -17, -20, -22, -24, -27, -29, -32, -34, -37, -39, -41, -44, -46, -48, -51, -53, -55, -58, -60, -62, -64, -66, -69, -71, -73, -75, -77, -79, -81, -83, -85, -86, -88, -90, -92, -93, -95, -97, -98, -100, -102, -103, -105, -106, -107, -109, -110, -111, -112, -114, -115, -116, -117, -118, -119, -120, -120, -121, -122, -123, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -123, -122, -121, -120, -120, -119, -118, -117, -116, -115, -114, -112, -111, -110, -109, -107, -106, -105, -103, -102, -100, -98, -97, -95, -93, -92, -90, -88, -86, -85, -83, -81, -79, -77, -75, -73, -71, -69, -66, -64, -62, -60, -58, -55, -53, -51, -48, -46, -44, -41, -39, -37, -34, -32, -29, -27, -24, -22, -20, -17, -15, -12, -10, -7, -5, -2 };


void sandbox_tick()
{
	paletteColor_t * pbo = GetPixelBuffer();

//	if( menu )
//	    drawMeleeMenu(disp, menu);
	int i = 0;


	memset( pbo,0, 280*240 );
	uint32_t v1, v2, v3, v4, v5, v6;
    SENS.sar_touch_conf.touch_data_sel = TOUCH_LL_READ_RAW;
    v1 = touches[touchhead][0] = SENS.sar_touch_status[9 - 1].touch_pad_data;
    v2 = touches[touchhead][1] = SENS.sar_touch_status[10 - 1].touch_pad_data;
    v3 = touches[touchhead][2] = SENS.sar_touch_status[11 - 1].touch_pad_data;
    v4 = touches[touchhead][3] = SENS.sar_touch_status[12 - 1].touch_pad_data;
    v5 = touches[touchhead][4] = SENS.sar_touch_status[13 - 1].touch_pad_data;
    v6 = touches[touchhead][5] = SENS.sar_touch_status[14 - 1].touch_pad_data;

	int peaker = -1;
	int vpeak = -1;

	for( i = 0; i < 5; i++ )
	{
		int v = touches[touchhead][i];
		if( v > vpeak ) { vpeak = v; peaker = i; }
	}
	int next = (peaker+1)%5;
	int prev = (peaker-1+5)%5;

	int vnext = touches[touchhead][next];
	int vprev = touches[touchhead][prev];


	vpeak = vpeak - 16000;
	vnext = vnext - 16000;
	vprev = vprev - 16000;

	int rotation = 256 * peaker;
	int intensity;
	if( vnext > vprev )
	{
		//Advancing further ahead
		intensity = vnext + vpeak + 1;
		//ESP_LOGI( "x", "%d %d\n", vnext, vpeak ); 
		rotation += vnext * 256 / intensity;
	}
	else
	{
		//Pulling back.
		intensity = vprev + vpeak + 1;
		rotation -= vprev * 256 / intensity;
	}


	ESP_LOGI( "x", "%5d %5d", intensity, rotation );


	touchhead = (touchhead+1) & 511;




	int colors[6] = { 16, 30, 160, 180, 4, 44 };
	int x = 0, y = 0;
	for( x = 0; x < 280; x++ )
	{
		uint8_t * pbx = &pbo[x]; 

		for( y = 0; y < 240; y++ )
		{
			pbx[y*280] = 0;
		}

		int ictx = 0;
		int th = (touchhead-x+511)&0x1ff;
		for( ictx = 0; ictx < 6; ictx++ )
		{
			uint32_t tp = touches[th][ictx];
			int v = (tp - 15000)/200;
			if( v < 0 ) v = 0;
			if( v > 239 ) v = 239;
			pbx[v*280] = colors[ictx];
		}
	}

	int dx = sinlut[((uint16_t)(rotation/4+320))%320]*intensity/90000;
	int dy = sinlut[((uint16_t)(rotation/4+80))%320]*intensity/90000;
	drawChar(disp, 255, meleeMenuFont.h, &meleeMenuFont.chars['O' - ' '], 140+dx, 120+dy );


	//ESP_LOGI( "x", "%d %d\n", rotation, intensity);
	// Draw Angle pointer.
//	esp_err_t e = touch_pad_read_raw_data(TOUCH_PAD_NUM9, &dat );
//	ESP_LOGI( "sandbox", "global_i: %d %d %d %d %d %d %d", global_i++, dat1, dat2, dat3, dat4, dat5, dat6 );
//	readok = 0;


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

