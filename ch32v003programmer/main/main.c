#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "tinyusb.h"
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
#include "tusb_cdc_acm.h"
#include "driver/uart.h"
// #include "nvs_flash.h"
// #include "nvs.h"

#include "funprog_config.h"
#include "rgb.h"
#include "settings.h"

#define SOC_DPORT_USB_BASE    0x60080000
#define SWAGTERM_REPORT_ID    0x0a
#define UART_NUMBER           1
#define UART_TX               17
#define UART_RX               18
#define UART_DTR              2
#define UART_DEFAULT_BAUDRATE 115200
#define LED_GPIO              14

static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

struct SandboxStruct * g_SandboxStruct;

nvs_handle_t storage;
uint8_t settings = 0;

// clang-format off
#define TUD_HID_REPORT_DESC_FUNPROG(...) \
	HID_USAGE_PAGE ( 0xff )                                       ,\
	HID_USAGE      ( 0x00 )                                       ,\
	HID_REPORT_SIZE  ( 8                                        ) ,\
	HID_COLLECTION ( HID_COLLECTION_APPLICATION )                 ,\
		HID_REPORT_COUNT ( 8                                      ) ,\
		/* These two for use with sandbox. IDs changed, will require change in sandbox tools too */ \
		HID_REPORT_ID    ( 0x0a                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		HID_REPORT_COUNT ( 254                                    ) ,\
		/* Used for swagterm */ \
		HID_REPORT_ID    ( 0x0b                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		HID_REPORT_COUNT ( 1                                      ) ,\
		/* Used for swagterm */ \
		HID_REPORT_ID    ( 0x0c                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		/* Allow for 0xaa (regular size), 0xab (jumbo sized) and 0xac mini feature reports; Windows needs specific id'd and size'd endpoints. */ \
		/* For compatibility with LS USB programmers */ \
		HID_REPORT_COUNT ( 7                                      ) ,\
		HID_REPORT_ID    ( 0xaa                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		HID_REPORT_COUNT ( (63)                                   ) ,\
		HID_REPORT_ID    ( 0xab                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		HID_REPORT_COUNT ( 127                                    ) ,\
		HID_REPORT_ID    ( 0xac                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		/* Legacy size for backwards compatibility on Windows and macos */ \
		HID_REPORT_COUNT ( 254                                    ) ,\
		HID_REPORT_ID    ( 0xad                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
		HID_REPORT_COUNT_N ( 263, 2                               ) ,\
		HID_REPORT_ID    ( 0xae                                   ) \
		HID_USAGE        ( HID_USAGE_DESKTOP_POINTER              ) ,\
		HID_FEATURE      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
	HID_COLLECTION_END
// clang-format on

static const uint8_t hid_report_descriptor[] = {TUD_HID_REPORT_DESC_FUNPROG()};

/**
 * @brief String descriptor
*/
static const char* hid_string_descriptor[7] = {
	// array of pointer to string descriptors
	(char[]){0x09, 0x04},                 // 0: is supported language is English (0x0409)
	"CNLohr",                             // 1: Manufacturer
	"ESP32-S2 CH32V003Programmer",        // 2: Product
	"s2-ch32xx-pgm-v5",                   // 3: Serials, overwritten with chip ID
	"CNLohr ESP32-S2 CH32V003Programmer", // 4: HID

	// Tricky keep these symbols, for sandboxing.  These are not used.  But, by keeping them here it makes them
	// accessable via the sandbox.
	(char*)&tud_connect, (char*)&tud_disconnect};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t config_descriptor[ ] =
{
	/* Configuration Descriptor */
	0x09,                                                   // bLength
	0x02,                                                   // bDescriptorType
	0x5c, 0x00,                                             // wTotalLength
	0x03,                                                   // bNumInterfaces (3)
	0x01,                                                   // bConfigurationValue
	0x00,                                                   // iConfiguration
	0xA0,                                                   // bmAttributes: Bus Powered; Remote Wakeup
	0x32,                                                   // MaxPower: 100mA

	0x09,        // bLength
	0x04,        // bDescriptorType - Interface
	0x00,        // bInterfaceNumber - 0
	0x00,        // bAlternateSetting
	0x01,        // bNumEndpoints - 1
	0x02,        // bInterfaceClass - CDC
	0x02,        // bInterfaceSubClass - Abstract Control Model (Table 4 in CDC120.pdf)
	0x01,        // bInterfaceProtocol - AT Commands: V.250 etc (Table 5)
	0x00,        // iInterface (String Index)

	// Setting up CDC interface (Table 18)
	0x05,        // bLength
	0x24,        // bDescriptorType - CS_INTERFACE (Table 12)
	0x00,        // bDescriptorSubType - Header Functional Descriptor (Table 13)
	0x10, 0x01,  // bcdCDC - USB version - USB1.1
	// Call Management Functional Descriptor
	0x05,        // bLength
	0x24,        // bDescriptorType - CS_INTERFACE
	0x01,        // bDescriptorSubType - Call Management Functional Descriptor (Table 13)
	0x00,        // bmCapabilities: (Table 3 in PSTN120.pdf)
	// Bit 0 — Device handles call management itself:
	//  1 = device handles call management (e.g. call setup, termination, etc.)
	//  0 = host handles it
	// Bit 1 — Device can send/receive call management information over a Data Class interface:
	//  1 = can use the Data Class interface for call management
	//  0 = must use the Communication Class interface
	0x01,        // bDataInterface - Indicates that multiplexed commands are handled via data interface 01h (same value as used in the UNION Functional Descriptor)
	// Abstract Control Management Functional Descriptor
	0x04,        // bLength
	0x24,        // bDescriptorType - CS_INTERFACE
	0x02,        // bDescriptorSubType - Abstract Control Management Functional Descriptor (Table 13)
	0x02,        // bmCapabilities - Device supports the request combination of Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding, and the notification Serial_State (Table 4 in PSTN120.pdf)
	// Union Descriptor Functional Descriptor
	0x05,        // bLength
	0x24,        // bDescriptorType - CS_INTERFACE
	0x06,        // bDescriptorSubType - Union Descriptor Functional Descriptor (Table 13)
	0x00,        // bControlInterface (Interface number of the control (Communications Class) interface)
	0x01,        // bSubordinateInterface0 (Interface number of the subordinate (Data Class) interface)
	// Setting up EP1 for CDC config interface
	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x03,        // bmAttributes (Interrupt)
	0x40, 0x00,  // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)

	// Transmission interface with two bulk endpoints
	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x01,        // bInterfaceNumber 1
	0x00,        // bAlternateSetting
	0x02,        // bNumEndpoints 2
	0x0A,        // bInterfaceClass
	0x00,        // bInterfaceSubClass
	0x00,        // bInterfaceProtocol - Transparent
	0x00,        // iInterface (String Index)
	// EP2 - device to host
	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x02,        // bEndpointAddress (OUT/H2D)
	0x02,        // bmAttributes (Bulk)
	0x40, 0x00,  // wMaxPacketSize 64
	0x00,        // bInterval 0 (unit depends on device speed)
	// EP3 - host to device
	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x83,        // bEndpointAddress (IN/D2H)
	0x02,        // bmAttributes (Bulk)
	0x40, 0x00,  // wMaxPacketSize 64
	0x00,        // bInterval 0 (unit depends on device speed)

	/* Interface Descriptor (HIDAPI) */
	0x09,                                                   // bLength
	0x04,                                                   // bDescriptorType
	0x02,                                                   // bInterfaceNumber
	0x00,                                                   // bAlternateSetting
	0x01,                                                   // bNumEndpoints
	0x03,                                                   // bInterfaceClass
	0x00,                                                   // bInterfaceSubClass
	0xff,                                                   // bInterfaceProtocol: Other
	0x00,                                                   // iInterface

	/* HID Descriptor (HIDAPI) */
	0x09,                                                   // bLength
	0x21,                                                   // bDescriptorType
	0x10, 0x01,                                             // bcdHID
	0x00,                                                   // bCountryCode
	0x01,                                                   // bNumDescriptors
	0x22,                                                   // bDescriptorType
	sizeof(hid_report_descriptor), 0x00,                    // wDescriptorLength

	/* Endpoint Descriptor (HIDAPI) */
	0x07,                                                   // bLength
	0x05,                                                   // bDescriptorType
	0x85,                                                   // bEndpointAddress: IN Endpoint 1
	0x03,                                                   // bmAttributes
	0x08, 0x00,                                             // wMaxPacketSize
	0xff,                                                   // bInterval: 255mS
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
	.configuration_descriptor = config_descriptor,
};

const void * c_descriptor;

static void uart_apply_config(int baud, int data, int stop, char parity)
{
	uart_config_t uart_config = {
		.baud_rate = baud,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	switch (data) {
		case 5:
			uart_config.data_bits = UART_DATA_5_BITS;
			break;
		case 6:
			uart_config.data_bits = UART_DATA_6_BITS;
			break;
		case 7:
			uart_config.data_bits = UART_DATA_7_BITS;
			break;
		case 8:
			uart_config.data_bits = UART_DATA_8_BITS;
			break;
	}
	switch (parity) {
		case 'E':
			uart_config.parity = UART_PARITY_EVEN;
			break;
		case 'O':
			uart_config.parity = UART_PARITY_ODD;
			break;
		default:
			uart_config.parity = UART_PARITY_DISABLE;
			break;
	}
	switch (stop) {
		case 2:
			uart_config.stop_bits = UART_STOP_BITS_2;
			break;
		default:
			uart_config.stop_bits = UART_STOP_BITS_1;
			break;
	}
	ESP_ERROR_CHECK(uart_param_config(UART_NUMBER, &uart_config));
}

static void configure_dtr_output(void)
{
	gpio_config_t dtr_cfg = {
		.pin_bit_mask = 1ULL << UART_DTR,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK(gpio_config(&dtr_cfg));
	gpio_set_level(UART_DTR, 0);
}

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
	size_t rx_size = 0;

	esp_err_t ret = tinyusb_cdcacm_read(itf, rx_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
	if (ret == ESP_OK) {
		// Forward bytes to UART
		int tx_len = uart_write_bytes(UART_NUMBER, (const char *) rx_buf, rx_size);
		if (tx_len < 0) {
			uprintf("UART write error\n");
		}
		else {
			// ws2812_pulse_color(0, 64, 0); // Green for inbound CDC traffic
		}
	}
	else {
		uprintf("CDC read error\n");
	}
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
	int dtr = event->line_state_changed_data.dtr;

	gpio_set_level(UART_DTR, !!dtr);
	uprintf("Line state changed on channel %d: DTR:%d", itf, dtr);
}

static void tinyusb_cdc_line_coding_changed_callback(int itf, cdcacm_event_t *event)
{
	cdc_line_coding_t const *coding = event->line_coding_changed_data.p_line_coding;
	int baud = coding->bit_rate;
	int data = coding->data_bits;
	int stop = coding->stop_bits;
	char parity = 'N';
	switch (coding->parity) {
		case 1:
			parity = 'O';
			break;
		case 2:
			parity = 'E';
			break;
		default:
			parity = 'N';
			break;
	}
	uart_apply_config(baud, data, stop, parity);
	uprintf("UART reconfigured by CDC: %d baud, %d data, %d stop, parity %c\n", baud, data, stop, parity);
}

static void uart_to_cdc_task(void *pvParameters)
{
	uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
	while (1) {
		int len = uart_read_bytes(UART_NUMBER, buf, sizeof(buf), pdMS_TO_TICKS(20));
		if (len > 0) {
			// Send to CDC (channel 0)
			tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, buf, len);
			esp_err_t err = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
			if (err != ESP_OK) {
				uprintf("CDC ACM write flush error (UART->CDC): %s\n", esp_err_to_name(err));
			}
			else {
				// ws2812_pulse_color(64, 0, 0); // Red for outbound CDC traffic
			}
		}
		// Small delay to avoid monopolizing the CPU
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void enable_uart(uint8_t enable)
{
	if (!enable && xTaskGetHandle("cdc_uart"))
	{
		vTaskDelete(xTaskGetHandle("cdc_uart"));
	}
	else if (enable && xTaskGetHandle("cdc_uart") == NULL)
	{
		xTaskCreate(uart_to_cdc_task, "cdc_uart", 4096, NULL, 10, NULL);
	}
}

void initUART()
{
	uart_config_t uart_config = {
		.baud_rate = UART_DEFAULT_BAUDRATE,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};

	ESP_ERROR_CHECK(uart_driver_install (UART_NUMBER, 1024, 1024, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config (UART_NUMBER, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin (UART_NUMBER, UART_TX, UART_RX, 0, 0));

	enable_uart(GET_UART_SETTING);
	// xTaskCreate(uart_to_cdc_task, "cdc_uart", 4096, NULL, 10, NULL);
}

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
	// if (report_id != 0xac) uprintf( "tud_hid_get_report_cb %02x %d\n", report_id, reqLen );
	int ret = reqLen;
	if (report_id == 0x0a)
	{
		ret = handle_advanced_usb_terminal_get(buffer - 1, reqLen + 1);
	}
	else if ((report_id == 0xaa || report_id == 0xab || report_id == 0xac || report_id == 0xad || report_id == 0xae) && advancedUsbHandler)
	{
		ret = advancedUsbHandler(buffer - 1, reqLen + 1, 1);
	}
	// uprintf( "RET: %d\n", ret );
	return ret;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type __attribute__((unused)), uint8_t const* buffer, uint16_t bufsize)
{
	// uprintf( "tud_hid_set_report_cb %02x %d\n", report_id, bufsize );
	if ((report_id == 0xaa || report_id == 0xab || report_id == 0xac || report_id == 0xad || report_id == 0xae) && advancedUsbHandler)
	{
		advancedUsbHandler((uint8_t*)buffer - 1, bufsize + 1, 0);
	}
	// uprintf( "SET DONE\n" );
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

void storage_init()
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// Erase and retry if partition was truncated or version changed
		err = nvs_flash_erase();
		uprintf("Error (%s) erasing NVS\n", esp_err_to_name(err));
		err = nvs_flash_init();
		uprintf("Error (%s) initializing NVS\n", esp_err_to_name(err));
		if (err != ESP_OK) return;
	}

	// Open an NVS namespace in Read/Write mode
	err = nvs_open("storage", NVS_READWRITE, &storage);
	if (err != ESP_OK)
	{
		uprintf("Error (%s) opening NVS handle!", esp_err_to_name(err));
		return;
	}

	err = nvs_get_u8(storage, "settings", &settings);
	if (err == ESP_OK) {
		uprintf("Loaded settings = %02x\n", settings);
	} else if (err == ESP_ERR_NVS_NOT_FOUND) {
		uprintf("No stored setting found\n");
		settings = DEFAULT_SETTINGS;
		nvs_set_u8(storage, "settings", settings);
		err = nvs_commit(storage);
		if (err != ESP_OK) uprintf("Error (%s) saving settings to NVS!\n", esp_err_to_name(err));
	} else {
		uprintf("Error (%s) reading settings!\n", esp_err_to_name(err));
	}

	uint8_t brightness = 0;
	err = nvs_get_u8(storage, "max_brightness", &brightness);
	if (err == ESP_OK) {
		uprintf("Loaded brightness setting = %u\n", brightness);
		leds_set_max_brightness(brightness);
	} else {
		uprintf("Error (%s) reading brightness setting!\n", esp_err_to_name(err));
	}
}

void app_main(void)
{
	printf("Hello world! Keep table at %p\n", &keep_symbols );

	g_SandboxStruct = &sandbox_mode;

	storage_init();

	configure_dtr_output ();

#if defined(USE_CDC_UART) && USE_CDC_UART
	initUART();
#endif

#if defined(ENABLE_RGB) && ENABLE_RGB
	initLEDS(LED_GPIO, 3);
#endif

	// Initialize TinyUSB with the default descriptor
	initTusb(&programmer_tusb_cfg, hid_report_descriptor);

	advancedUsbHandler = g_SandboxStruct->fnAdvancedUSB;

	esp_log_set_vprintf( advanced_usb_write_log_printf );

	// esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_OFF);
	esp_efuse_set_rom_log_scheme(ESP_EFUSE_ROM_LOG_ALWAYS_ON);

	printf("Minimum free heap size: %d bytes\n", (int)esp_get_minimum_free_heap_size());

	tinyusb_config_cdcacm_t acm_cfg = {
		.cdc_port = TINYUSB_CDC_ACM_0,
		.callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
		.callback_rx_wanted_char = NULL,
		.callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
		.callback_line_coding_changed = &tinyusb_cdc_line_coding_changed_callback
	};

	ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
	/* the second way to register a callback */
	ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(TINYUSB_CDC_ACM_0, CDC_EVENT_LINE_STATE_CHANGED, &tinyusb_cdc_line_state_changed_callback));

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
			// tud_hid_gamepad_report(HID_ITF_PROTOCOL_NONE, 0, 0, 0, 0, 0, 0, 0, 0);
		}
	} while( 1 );

}
