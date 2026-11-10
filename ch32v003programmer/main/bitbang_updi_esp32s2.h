// UPDI reference implementation for bit banged IO on the ESP32-S2.
//
// Copyright 2024 Charles Lohr, May be licensed under the MIT/x11 or NewBSD
// licenses.  You choose.  (Can be included in commercial and copyleft work)
//
// This file is original work.
//

// General notes:
//   Default UPDI Setting is UPDI @ 4MHz
//     -> 0.75 - 225kbps
//   Let's target 100 kbps.

#ifndef BITBANG_UPDI_ESP32S2_H
#define BITBANG_UPDI_ESP32S2_H

static inline void UPDIPinSetup( struct UPDIState* updi )
{
	REG_WRITE( IO_MUX_GPIO6_REG, 1<<FUN_IE_S | 1<<FUN_DRV_S );  //Additional pull-up, 10mA drive.  Optional: 10k pull-up resistor.
	REG_WRITE( IO_MUX_GPIO7_REG, 1<<FUN_IE_S | 3<<FUN_DRV_S );  //VCC for part 40mA drive.
}

static inline uint32_t GetCPUClock(void)
{
	rtc_cpu_freq_config_t m;
	rtc_clk_cpu_freq_get_config( &m );
	return m.freq_mhz;
}

static inline uint32_t GetCCount(void)
{
	unsigned r;
	asm volatile ("rsr %0, ccount" : "=r"(r));
	return r;
}

static inline void DelayCycles( uint32_t cycles )
{
	asm volatile ("\n\
	rsr a3, ccount\n\
	add a3, a3, %[cycwait]\n\
1:\n\
	rsr a4, ccount\n\
	blt a4, a3, 1b\n\
	" : : [cycwait]"r"(cycles) : "a3", "a4" );
}

static inline void DelayUntil( uint32_t cuntil )
{
	asm volatile ("\n\
1:\n\
	rsr a3, ccount\n\
	blt a3, %[cuntil], 1b\n\
	" : : [cuntil]"r"(cuntil) : "a3" );
}

static inline void UPDIWordInternal( struct UPDIState* updi, uint16_t word )
{
	int i;
	DisableISR();
	uint32_t Ctarget = GetCCount() + updi->clocks_per_bit;
	GPIO.out_w1ts = updi->pinmask;
	GPIO.enable_w1ts = updi->pinmask;
	DelayUntil( Ctarget );
	Ctarget += updi->clocks_per_bit;
	for( i = 0; i < 12; i++ )
	{
		volatile uint32_t * gp = (word&1) ? &GPIO.out_w1ts : &GPIO.out_w1tc;
		*gp = updi->pinmask;
		word>>=1;
		//esp_rom_delay_us(10); // target 100kbps
		DelayUntil( Ctarget );
		Ctarget += updi->clocks_per_bit;
	}
	GPIO.enable_w1tc = updi->pinmask;
	EnableISR();
	DelayUntil( Ctarget ); // second stop bit.
}

// Currently unused
static inline void UPDISendBreak( struct UPDIState* updi )
{
	GPIO.out_w1tc = updi->pinmask;
	GPIO.enable_w1ts = updi->pinmask;
	esp_rom_delay_us(24600); // Table 32-3, attiny416 datashet, worst case scenario
	GPIO.out_w1ts = updi->pinmask;
	 asm volatile( "nop" ); asm volatile( "nop" ); asm volatile( "nop" );// Tiny delay.
	GPIO.enable_w1tc = updi->pinmask;
	esp_rom_delay_us(200);
}

static inline int UPDISyncStart( struct UPDIState* updi )
{
	DisableISR();
	GPIO.enable_w1tc = updi->pinmask;
	GPIO.out_w1ts = updi->pinmask;
	esp_rom_delay_us(1);
	int linebit = GPIO.in & updi->pinmask;
	if( !linebit )
		return -1;

	GPIO.out_w1tc = updi->pinmask;
	GPIO.enable_w1ts = updi->pinmask;

	// 4.167ns per cycle, ~48 cycles, hold low for 200ns (only sync tip)
	DelayCycles( updi->cpumhz/5 ); 

	// Release line
	GPIO.enable_w1tc = updi->pinmask;
	GPIO.out_w1ts = updi->pinmask;

	// Wait just a tiiiny bit.
	esp_rom_delay_us(1);

	// Make sure the AVR is still initializing after 2us.
	int ackbit = GPIO.in & updi->pinmask;
	EnableISR();

	// 10-200 us later, the AVR releases the line.
	// (marker 2 on Figure 33-5 in ATTiny416 datasheet)
	esp_rom_delay_us(200);
	if( ackbit )
		return -2;
	return 0;	
}

static int UPDIReceive( struct UPDIState* updi )
{
	// Make sure it's in High-Z
	GPIO.enable_w1tc = updi->pinmask;
	GPIO.out_w1ts = updi->pinmask;
	DisableISR();

	int32_t lastNow = GetCCount();

	int32_t nextEvent = lastNow + (updi->clocks_per_bit << 9);

	while( GPIO.in & updi->pinmask )
	{
		lastNow = GetCCount();
		if ( lastNow > nextEvent )
		{
			EnableISR();
			return -4;
		}
	}

	// 1.5 bits in the future is when we want to sample.
	nextEvent = lastNow + updi->clocks_per_bit + (updi->clocks_per_bit>>1);

	int i;
	uint32_t word = 0;
	int parity = 0;
	for( i = 0; i < 10; i++ )
	{
		DelayUntil( nextEvent );
		nextEvent += updi->clocks_per_bit;
		word >>= 1;
		if( GPIO.in & updi->pinmask )
		{
			word |= 0x200;
			if( i < 9 )
			{
				parity = !parity;
			}
		}
	}

	EnableISR();

	if( parity )
	{
		return -5;
	}

	// stop bit.
	int check = word >> 9;
	if( !check )
		return -6;

	return word & 0xff;
}

static inline void UPDIPowerOn( struct UPDIState* updi )
{
	GPIO.enable_w1tc = updi->pinmask;     // High-Z the pinmask
	GPIO.enable_w1ts = updi->pinmaskpower;
	GPIO.out_w1ts = updi->pinmaskpower;
}

static inline void UPDIPowerOff( struct UPDIState* updi )
{
	GPIO.enable_w1tc = updi->pinmask;     // Keep pinmask in high-z
	GPIO.enable_w1ts = updi->pinmaskpower;
	GPIO.out_w1tc = updi->pinmaskpower;
}

#endif
