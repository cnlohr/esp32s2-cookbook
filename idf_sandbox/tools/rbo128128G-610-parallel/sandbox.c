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


// Example talking to the RBO128128G-610 using the MC6800 interface with the ST7571

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

#define LCD_CS   1
#define LCD_RST  2
#define LCD_DA0  3
#define LCD_RDWR 4
#define LCD_E_RD 5
#define LCD_DB0  6
#define LCD_DB1  7
#define LCD_DB2  8
#define LCD_DB3  9
#define LCD_DB4  10
#define LCD_DB5  11
#define LCD_DB6  12
#define LCD_DB7  13


static void Reset( )
{
	GPIO.out_w1tc = (1<<LCD_RST) | (1<<LCD_RDWR); //Assert reset
	GPIO.out_w1ts = (1<<LCD_CS) | (1<<LCD_DA0) | (1<<LCD_E_RD) |
		(1<<LCD_DB0) | (1<<LCD_DB1) | (1<<LCD_DB2) | (1<<LCD_DB3) | (1<<LCD_DB4) |
		(1<<LCD_DB5) | (1<<LCD_DB6) | (1<<LCD_DB7);
	GPIO.enable_w1ts = (1<<LCD_CS) | (1<<LCD_DA0) | (1<<LCD_RDWR) | (1<<LCD_E_RD) |
		(1<<LCD_DB0) | (1<<LCD_DB1) | (1<<LCD_DB2) | (1<<LCD_DB3) | (1<<LCD_DB4) |
		(1<<LCD_DB5) | (1<<LCD_DB6) | (1<<LCD_DB7) | (1<<LCD_RST);
	esp_rom_delay_us(100);
	GPIO.out_w1ts = 1<<LCD_RST; //Release reset
	esp_rom_delay_us(100);
	GPIO.out_w1tc = 1<<LCD_CS; //Enable chip
	esp_rom_delay_us(100);
}

static void Delay( int ms )
{
	esp_rom_delay_us(ms*1000);
}

#define COMMAND 0
#define DATA    1

static void Write( int type, uint8_t value )
{
	uint32_t extraon = 0;
	uint32_t extraoff = 0;
	GPIO.out_w1ts = (1<<LCD_E_RD);

	if( type == 0 )
		extraoff = 1<<LCD_DA0;
	else
		extraon = 1<<LCD_DA0;

	esp_rom_delay_us(20);
	GPIO.out_w1ts = (value<<LCD_DB0) | extraon;
	GPIO.out_w1tc = (((~value)&0xff)<<LCD_DB0) | extraoff;
	esp_rom_delay_us(20);
	GPIO.out_w1tc = (1<<LCD_E_RD);
}

static void Initial_ST7571(void)
{
	Reset( );
	Delay (100); // Delay 100ms for stable VDD1/VDD2/VDD3
	Write(COMMAND, 0xAE); // Display OFF
	Write(COMMAND, 0xE1); // Software reset
	Write(COMMAND, 0x38); // MODE SET
	Write(COMMAND, 0x98); // FR=1010 => 80Hz,  BE = Booster Efficiency Level 3
	// BE[1:0]=1,0 => booster efficiency Level-3
	Write(COMMAND, 0xA1); // ADC select, ADC=1 =>reverse direction
	Write(COMMAND, 0xC8); // SHL select, SHL=1 => reverse direction
	Write(COMMAND, 0x44); // Set initial COM0 register
	Write(COMMAND, 0x00); //
	Write(COMMAND, 0x40); // Set initial display line register
	Write(COMMAND, 0x00); //
	Write(COMMAND, 0xAB); // OSC. ON

	Write(COMMAND, 0x67); // DC-DC step up, 8 times boosting circuit
	Write(COMMAND, 0x25); // Select regulator register(1+(Ra+Rb))

	Write(COMMAND, 0x81); // Set Reference Voltage "Set Electronic Volume Register"
	Write(COMMAND, 0x1c); // Midway up.

	Write(COMMAND, 0x52); // Set LCD Bias=1/8 V0 (Experimentally found)

	Write(COMMAND, 0xF3); // Release Bias Power Save Mode
	Write(COMMAND, 0x04); //
	Write(COMMAND, 0x83); // Set FRC and PWM mode (4FRC & 15PWM)

	Write(COMMAND, 0x2C); // Power Control, VC: ON VR: OFF VF: OFF
	Delay (200); // Delay 200ms
	Write(COMMAND, 0x2E); // Power Control, VC: ON VR: ON VF: OFF
	Delay (200); // Delay 200ms
	Write(COMMAND, 0x2F); // Power Control, VC: ON VR: ON VF: ON
	Delay (10); // Delay 10ms
	Write(COMMAND, 0xAF); // Display ON
}

void sandbox_main()
{
	uprintf( "sandbox_main()\n" );
	
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_CS,   1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_RST,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DA0,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_RDWR, 1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_E_RD, 1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB0,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB1,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB2,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB3,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB4,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB5,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB6,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_DB7,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
}


void sandbox_tick()
{
	uint32_t start = getCycleCount();
	uint32_t end = getCycleCount();

	Initial_ST7571();


// Set Display Start Line
	Write( COMMAND, 0x40 ); Write( COMMAND, 0 );
// Set Page Address
	Write( COMMAND, 0xB0 ); // Page address = 0
// Set Column Address = 0
	Write( COMMAND, 0x10 ); // MSB
	Write( COMMAND, 0x00 ); // LSB
// Write Display Data
	int i;
	for( i = 0; i < 1024; i++ )
		Write( DATA, 0xff );

Write(COMMAND, 0xA7); // Invert Display

	uprintf( "%d %p %p\n", end-start, IO_MUX_GPIO0_REG + 0, IO_MUX_GPIO0_REG + 1 );
	vTaskDelay( 200 );

Write(COMMAND, 0xA6); //Un-Invert Display
	//vTaskDelay( 200 );

}

struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnAdvancedUSB = NULL
};
