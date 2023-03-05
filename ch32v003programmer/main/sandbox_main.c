#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h" // or "esp_efuse_custom_table.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include "advanced_usb_control.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "soc/dedic_gpio_reg.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "soc/usb_reg.h"
#include "ulp_riscv.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/rtc.h"

#define SOC_DPORT_USB_BASE 0x60080000

struct SandboxStruct * g_SandboxStruct;

uint16_t tud_hid_get_report_cb(uint8_t itf,
							   uint8_t report_id,
							   hid_report_type_t report_type,
							   uint8_t* buffer,
							   uint16_t reqlen)
{
	if( report_id == 170 || report_id == 171 )
	{
		return handle_advanced_usb_control_get( reqlen, buffer );
	}
	else if( report_id == 172 )
	{
		return handle_advanced_usb_terminal_get( reqlen, buffer );
	}
	else if( report_id == 173 && g_SandboxStruct && g_SandboxStruct->fnAdvancedUSB )
	{
		return g_SandboxStruct->fnAdvancedUSB( buffer, reqlen, 1 );
	}
	else
	{
		return reqlen;
	}
}

void tud_hid_set_report_cb(uint8_t itf,
						   uint8_t report_id,
						   hid_report_type_t report_type,
						   uint8_t const* buffer,
						   uint16_t bufsize )
{
	if( report_id >= 170 && report_id <= 171 )
	{
		handle_advanced_usb_control_set( bufsize, buffer );
	}
	else if( report_id == 173 && g_SandboxStruct && g_SandboxStruct->fnAdvancedUSB )
	{
		g_SandboxStruct->fnAdvancedUSB( (uint8_t*)buffer, bufsize, 0 );
	}
}

void esp_sleep_enable_timer_wakeup();

volatile void * keep_symbols[] = { 0, uprintf, vTaskDelay, ulp_riscv_halt,
	ulp_riscv_timer_resume, ulp_riscv_timer_stop, ulp_riscv_load_binary,
	ulp_riscv_run, ulp_riscv_config_and_run, esp_sleep_enable_timer_wakeup,
	ulp_set_wakeup_period, rtc_gpio_init, rtc_gpio_set_direction,
	rtc_gpio_set_level, gpio_config, gpio_matrix_out, gpio_matrix_in,
	rtc_clk_cpu_freq_get_config, rtc_clk_cpu_freq_set_config_fast,
	rtc_clk_apb_freq_get };

void app_main(void)
{
	printf("Hello world! Keep table at %p\n", &keep_symbols );

	esp_log_set_vprintf( advanced_usb_write_log_printf );
	
	/* The ESP32-C3 can enumerate as a USB CDC device using pins 18 and 19
	 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/usb-serial-jtag-console.html
	 *
	 * This is enabled or disabled with idf.py menuconfig -> "Channel for console output"
	 *
	 * When USB JTAG is enabled, the ESP32-C3 will halt and wait for a reception
	 * any time it tries to transmit bytes on the UART. This means a program
	 * won't run if a console isn't open, because the IDF tries to print stuff
	 * on boot.
	 * To get around this, use the efuse bit to permanently disable logging
	 */
	// esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_OFF);
	esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_ON);

    tinyusb_config_t tusb_cfg = {};
    tinyusb_driver_install(&tusb_cfg);
        
	printf("Minimum free heap size: %d bytes\n", (int)esp_get_minimum_free_heap_size());

	do
	{
		if( g_SandboxStruct && g_SandboxStruct->fnIdle ) { g_SandboxStruct->fnIdle(); }
		esp_task_wdt_reset();
		taskYIELD();
	} while( 1 );

//	printf("Restarting now.\n");
//	fflush(stdout);
//	esp_restart();
}
