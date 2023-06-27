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


// Example talking to the RBO128128G-610 using the MC6800 interface.

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
#define LCD_SCK  12
#define LCD_SDA  13

int is_getting_data_from_host;


static void Reset( )
{
	GPIO.out_w1tc = (1<<LCD_RST); //Assert reset
	GPIO.out_w1ts = (1<<LCD_CS) | (1<<LCD_DA0) | (1<<LCD_E_RD) | (1<<LCD_RDWR) | (1<<LCD_SCK) | (1<<LCD_SDA) |
		(1<<LCD_DB0) | (1<<LCD_DB1) | (1<<LCD_DB2) | (1<<LCD_DB3) | (1<<LCD_DB4) | (1<<LCD_DB5 );
	GPIO.enable_w1ts = (1<<LCD_RST) | (1<<LCD_CS) | (1<<LCD_DA0) | (1<<LCD_RDWR) | (1<<LCD_E_RD) | (1<<LCD_SCK) | (1<<LCD_SDA) |
		(1<<LCD_DB0) | (1<<LCD_DB1) | (1<<LCD_DB2) | (1<<LCD_DB3) | (1<<LCD_DB4) | (1<<LCD_DB5 );
	esp_rom_delay_us(10000);
	GPIO.out_w1ts = 1<<LCD_RST; //Release reset
	esp_rom_delay_us(10000);
//	GPIO.out_w1tc = 1<<LCD_CS; //Enable chip
//	esp_rom_delay_us(10000);
}

static void Delay( int ms )
{
	esp_rom_delay_us(ms*1000);
}

#define COMMAND 0
#define DATA    1

static void Write( int type, uint32_t value )
{
	if( type == COMMAND )
		GPIO.out_w1tc = 1<<LCD_DA0;
	else
		GPIO.out_w1ts = 1<<LCD_DA0;

	GPIO.out_w1tc = 1<<LCD_CS; //Enable chip


	int i;
	for( i = 0; i < 8; i++ )
	{
		if( value & 128 )
			GPIO.out_w1ts = 1<<LCD_SDA;
		else
			GPIO.out_w1tc = 1<<LCD_SDA;
		GPIO.out_w1tc = 1<<LCD_SCK;
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		GPIO.out_w1ts = 1<<LCD_SCK;
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		asm volatile( "nop; nop; nop; nop; nop" );
		value <<= 1;
	}

	GPIO.out_w1ts = 1<<LCD_CS; //Disable chip
}

static void Initial_ST7571(void)
{
	Reset( );
	Delay (100); // Delay 100ms for stable VDD1/VDD2/VDD3
	Write(COMMAND, 0xAE); // Display OFF
	Write(COMMAND, 0xE1); // Software reset
	Write(COMMAND, 0x38); // MODE SET
	Write(COMMAND, 0xe8); // FR=1010 => 80Hz,  BE = Booster Efficiency Level 3
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
	Write(COMMAND, 0x04); // ???
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
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_SCK,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );
	REG_WRITE( IO_MUX_GPIO0_REG + 4*LCD_SDA,  1<<FUN_IE_S | 0<<FUN_PU_S | 2<<FUN_DRV_S );

	Initial_ST7571();

	Delay (10); // Delay 10ms

	Write( COMMAND, 0x00 ); // Reset modes.

	Write( COMMAND, 0x7B ); // Enter weird modes.
	Write( COMMAND, 0x11 ); // B&W vs grey
	Write( COMMAND, 0x00 ); // Reset modes.


	Write( COMMAND, 0x40 ); Write( COMMAND, 0 );
	// Set Page Address
	Write( COMMAND, 0xB0 ); // Page address = 0
	// Set Column Address = 0
	Write( COMMAND, 0x10 ); // MSB
	Write( COMMAND, 0x00 ); // LSB
	// Write Display Data
	int i;
	for( i = 0; i < 2048; i++ )
		Write( DATA, 0xaa );
//	stop = getCycleCount();

	Write(COMMAND, 0xAF); // Display ON


	is_getting_data_from_host = 0;
}


void sandbox_tick()
{
/*	if( !is_getting_data_from_host )
	{
		uint32_t stop, start;
		int l;
		for( l = 0; l < 10; l++ )
		{
			start = getCycleCount();
		// Set Display Start Line
			Write( COMMAND, 0x40 ); Write( COMMAND, 0 );
		// Set Page Address
			Write( COMMAND, 0xB0 ); // Page address = 0
		// Set Column Address = 0
			Write( COMMAND, 0x10 ); // MSB
			Write( COMMAND, 0x00 ); // LSB
		// Write Display Data
			int i;
			for( i = 0; i < 2048; i++ )
				Write( DATA, (l&1)?0xff:0x00 );
			stop = getCycleCount();
			vTaskDelay( 30 );
		}
	//Write(COMMAND, 0xA7); // Invert Display

		uprintf( "%d %p %p\n", stop-start, IO_MUX_GPIO0_REG + 0, IO_MUX_GPIO0_REG + 1 );
		vTaskDelay( 200 );

		Write(COMMAND, 0xA6); //Un-Invert Display
		//vTaskDelay( 200 );
	}
	else*/
	{
		vTaskDelay( 10 );
	}
}

int get_usb_data( uint8_t * buffer, int reqlen, int is_get )
{
	if( is_get )
	{
		return 0;
	}

	// buffer[0] is the request ID.
	uint8_t * iptr = &buffer[1];

	int len = *(iptr++)+1;
	if( len > 256 ) len = 256;
	uint8_t page = *(iptr++);

	if( page >= 32 )
	{
		// Special commands.
		int i;
		for( i = 0; i < len; i++ )
		{
			uint8_t cmd = *(iptr++);
			if( cmd == 0xff ) break;
			Write( COMMAND, cmd ) ;
		}
	}
	else
	{
		// Set Display Start Line
		Write( COMMAND, 0x40 ); Write( COMMAND, 0 );
		Write( COMMAND, 0xB0 | (page&0xf) ); // Page address = 0
		Write( COMMAND, 0x10 + ((page&0x10)?4:0) ); // MSB
		Write( COMMAND, 0x00 ); // LSB

		int i;
		for( i = 0; i < len; i++ )
		{
			Write( DATA, *(iptr++) );
		}

		is_getting_data_from_host = 1;
	}

	return 0;
}


struct SandboxStruct sandbox_mode =
{
	.fnIdle = sandbox_tick,
	.fnAdvancedUSB = get_usb_data,
};
