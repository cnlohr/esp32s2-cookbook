#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
//#include "spi_flash_mmap.h"
//#include "esp_efuse_table.h" // or "esp_efuse_custom_table.h"
//#include "esp_efuse.h"
#include "tinyusb.h"
//#include "tusb_hid_gamepad.h"
#include "advanced_usb_control.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "soc/dedic_gpio_reg.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/system_reg.h"
#include "soc/usb_reg.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "soc/rtc.h"
#include "ulp_riscv.h"
#include "esp_efuse.h"

#define SOC_DPORT_USB_BASE 0x60080000

struct SandboxStruct * g_SandboxStruct;




// clang-format off
/**
 * @brief HID Gamepad Report Descriptor Template
 * with 32 buttons, 2 joysticks and 1 hat/dpad with following layout
 * | X | Y | Z | Rz | Rx | Ry (1 byte each) | hat/DPAD (1 byte) | Button Map (4 bytes) |
 */
#define TUD_HID_REPORT_DESC_GAMEPAD_SWADGE(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                 ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD  )                 ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
	HID_REPORT_ID( /*REPORT_ID_GAMEPAD*/ 0x01 ) \
    /* 8 bit X, Y, Z, Rz, Rx, Ry (min -127, max 127 ) */ \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_X                    ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_Y                    ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_Z                    ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_RZ                   ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_RX                   ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_RY                   ) ,\
    HID_LOGICAL_MIN  ( 0x81                                   ) ,\
    HID_LOGICAL_MAX  ( 0x7f                                   ) ,\
    HID_REPORT_COUNT ( 6                                      ) ,\
    HID_REPORT_SIZE  ( 8                                      ) ,\
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 8 bit DPad/Hat Button Map  */ \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_DESKTOP                 ) ,\
    HID_USAGE        ( HID_USAGE_DESKTOP_HAT_SWITCH           ) ,\
    HID_LOGICAL_MIN  ( 1                                      ) ,\
    HID_LOGICAL_MAX  ( 8                                      ) ,\
    HID_PHYSICAL_MIN ( 0                                      ) ,\
    HID_PHYSICAL_MAX_N ( 315, 2                               ) ,\
    HID_REPORT_COUNT ( 1                                      ) ,\
    HID_REPORT_SIZE  ( 8                                      ) ,\
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* 16 bit Button Map */ \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_BUTTON                  ) ,\
    HID_USAGE_MIN    ( 1                                      ) ,\
    HID_USAGE_MAX    ( 16                                     ) ,\
    HID_LOGICAL_MIN  ( 0                                      ) ,\
    HID_LOGICAL_MAX  ( 1                                      ) ,\
    HID_REPORT_COUNT ( 16                                     ) ,\
    HID_REPORT_SIZE  ( 1                                      ) ,\
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
    /* Allow for 0xaa (regular size), 0xab (jumbo sized) and 0xac mini feature reports; Windows needs specific id'd and size'd endpoints. */ \
    HID_REPORT_COUNT ( CFG_TUD_ENDPOINT0_SIZE                 ) ,\
    HID_REPORT_SIZE  ( 8                                      ) ,\
    HID_REPORT_ID    ( 0xaa                                   ) \
    HID_USAGE        ( HID_USAGE_DESKTOP_GAMEPAD              ) ,\
    HID_FEATURE      ( HID_DATA | HID_ARRAY | HID_ABSOLUTE    ) ,\
    HID_REPORT_COUNT ( (255-1)         ) ,\
    HID_REPORT_ID    ( 0xab                                   ) \
    HID_USAGE        ( HID_USAGE_DESKTOP_GAMEPAD              ) ,\
    HID_FEATURE      ( HID_DATA | HID_ARRAY | HID_ABSOLUTE    ) ,\
    HID_REPORT_COUNT ( 1                                      ) ,\
    HID_REPORT_ID    ( 0xac                                   ) \
    HID_USAGE        ( HID_USAGE_DESKTOP_GAMEPAD              ) ,\
    HID_FEATURE      ( HID_DATA | HID_ARRAY | HID_ABSOLUTE    ) ,\
    HID_REPORT_COUNT ( (255-1)         ) ,\
    HID_REPORT_ID    ( 0xad                                   ) \
    HID_USAGE        ( HID_USAGE_DESKTOP_GAMEPAD              ) ,\
    HID_FEATURE      ( HID_DATA | HID_ARRAY | HID_ABSOLUTE    ) ,\
  HID_COLLECTION_END
// clang-format on

static const uint8_t hid_report_descriptor[] = {TUD_HID_REPORT_DESC_GAMEPAD_SWADGE()};

/**
 * @brief String descriptor
 */
static const char* hid_string_descriptor[7] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},   // 0: is supported language is English (0x0409)
    "Magfest",              // 1: Manufacturer
    "Swadge Controller",    // 2: Product
    "s2-ch32xx-pgm-v0",               // 3: Serials, overwritten with chip ID
    "Swadge HID interface", // 4: HID

    // Tricky keep these symbols, for sandboxing.  These are not used.  But, by keeping them here it makes them
    // accessable via the sandbox.
    (char*)&tud_connect, (char*)&tud_disconnect};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
/// @brief PC Config Descriptor
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1,                                                        // Configuration number
                          1,                                                        // interface count
                          0,                                                        // string index
                          (TUD_CONFIG_DESC_LEN + (CFG_TUD_HID * TUD_HID_DESC_LEN)), // total length
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,                       // attribute
                          100),                                                     // power in mA

    TUD_HID_DESCRIPTOR(0,                             // Interface number
                       4,                             // string index
                       false,                         // boot protocol
                       sizeof(hid_report_descriptor), // report descriptor len
                       0x81,                          // EP In address
                       16,                            // size
                       10),                           // polling interval
};


static const tusb_desc_device_t knsDescriptor = {
    .bLength            = 18U,
    .bDescriptorType    = 1,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,
    .idVendor           = 0x303a,
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};


/// @brief PC tusb configuration
static const tinyusb_config_t programmer_tusb_cfg = {
    .device_descriptor        = &knsDescriptor,
    .string_descriptor        = hid_string_descriptor,
	.string_descriptor_count = sizeof(hid_string_descriptor)/sizeof(hid_string_descriptor[0]),
    .external_phy             = false,
    .configuration_descriptor = hid_configuration_descriptor,
};

#if 0
static const tinyusb_config_t tusb_cfg = {
    .device_descriptor        = NULL,
    .string_descriptor        = hid_string_descriptor,
    .external_phy             = false,
    .configuration_descriptor = hid_configuration_descriptor,
};
#endif

const void * c_descriptor;

void initTusb(const tinyusb_config_t* tusb_cfg, const uint8_t* descriptor)
{
    c_descriptor = descriptor;
    ESP_ERROR_CHECK(tinyusb_driver_install(tusb_cfg));
}
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance __attribute__((unused)))
{

    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return c_descriptor;
}


typedef int16_t (*fnAdvancedUsbHandler)(uint8_t* buffer, uint16_t length, uint8_t isGet);

static fnAdvancedUsbHandler advancedUsbHandler;

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type __attribute__((unused)), uint8_t* buffer, uint16_t reqLen)
{
    if (report_id == 170 || report_id == 171)
    {
        return handle_advanced_usb_control_get(buffer - 1, reqLen + 1);
    }
    else if (report_id == 172)
    {
        return handle_advanced_usb_terminal_get(buffer - 1, reqLen + 1);
    }
    else if (report_id == 173 && advancedUsbHandler)
    {
        return advancedUsbHandler(buffer - 1, reqLen + 1, 1);
    }
    else
    {
        return reqLen;
    }
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type __attribute__((unused)), uint8_t const* buffer, uint16_t bufsize)
{
    if (report_id >= 170 && report_id <= 171)
    {
        handle_advanced_usb_control_set(buffer - 1, bufsize + 1);
    }
    else if (report_id == 173 && advancedUsbHandler)
    {
       advancedUsbHandler((uint8_t*)buffer - 1, bufsize + 1, 0);
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

extern struct SandboxStruct sandbox_mode;

void app_main(void)
{
	printf("Hello world! Keep table at %p\n", &keep_symbols );

	g_SandboxStruct = &sandbox_mode;

    // Initialize TinyUSB with the default descriptor
    initTusb(&programmer_tusb_cfg, hid_report_descriptor);

	advancedUsbHandler = g_SandboxStruct->fnAdvancedUSB;

	esp_log_set_vprintf( advanced_usb_write_log_printf );

	// esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_OFF);
	esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_ON);

	printf("Minimum free heap size: %d bytes\n", (int)esp_get_minimum_free_heap_size());

	void sandbox_main();
	uprintf( "SETUP\n" );

	sandbox_main();

	do
	{
		if( g_SandboxStruct && g_SandboxStruct->fnIdle ) { g_SandboxStruct->fnIdle(); }
		esp_task_wdt_reset();
		taskYIELD();

		if (tud_ready())
		{
		    tud_hid_gamepad_report(HID_ITF_PROTOCOL_NONE, 0, 0, 0, 0, 0, 0, 0, 0);
		}


	} while( 1 );

//	printf("Restarting now.\n");
//	fflush(stdout);
//	esp_restart();
}
