#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "advanced_usb_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
//	ESP_LOGI( "sandbox", "Running from IRAM. %d", global_i );
//	ESP_LOGI( "sandbox", "Loaded" );
}

void sandbox_tick()
{
	uint32_t start = getCycleCount();
	uint32_t end = getCycleCount();
	uprintf( "%d\n", end-start );
	vTaskDelay( 1 );
}

struct SandboxStruct sandbox_mode =
{
	.idleFunction = sandbox_tick,
	.fnAdvancedUSB = NULL
};
