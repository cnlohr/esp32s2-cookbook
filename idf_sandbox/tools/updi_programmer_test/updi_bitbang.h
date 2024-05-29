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

#ifndef _UPDI_BITBANG_H
#define _UPDI_BITBANG_H


#define UPDI_WORD_ACK   0xe80
#define UPDI_WORD_REST  0x000 // RESET or like a RESET BREAK


static int UPDISetup( uint32_t pinmask, int cpumhz, int baudrate, int * clocks_per_bit_ret, uint8_t sib[16] );
static inline void UPDIPowerOff( int pinmask, int pinmaskpower );
static inline void UPDIPowerOn( int pinmask, int pinmaskpower );
static int UPDIErase( uint32_t pinmask, int clocks_per_bit );
static int UPDIActivateProgramming( uint32_t pinmask, int clocks_per_bit );
static int UPDIActivateUserRow( uint32_t pinmask, int clocks_per_bit );



// Returns 0 if device detected.  Returns nonzero if failure.
static inline int UPDISyncStart( int pinmask, int cpu_mhz );


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


static inline void UPDIWordInternal( int pinmask, int counts_per_bit, uint16_t word )
{
	int i;
	DisableISR();
	uint32_t Ctarget = GetCCount() + counts_per_bit;
	for( i = 0; i < 12; i++ )
	{

		volatile uint32_t * gp = (word&1) ? &GPIO.out_w1ts : &GPIO.out_w1tc;
		*gp = pinmask;
		GPIO.enable_w1ts = pinmask;
		word>>=1;
		//esp_rom_delay_us(10); // target 100kbps
		DelayUntil( Ctarget );
		Ctarget += counts_per_bit;
	}
	GPIO.enable_w1tc = pinmask;
	EnableISR();
	DelayUntil( Ctarget ); // second stop bit.
}

static inline void UPDISendWord( int pinmask, int counts_per_bit, uint32_t message )
{
	int parity = (message ^ (message>>4)) & 0xf;
	message <<= 1;
	message |= 0xc00;
	message |= (0xD32C00 >> parity) & 0x200;
	UPDIWordInternal( pinmask, counts_per_bit, message );
}

static inline void UPDISendString( int pinmask, int counts_per_bit, const uint8_t * str, int len )
{
	int i;
	for( i = 0; i < len; i++ )
		UPDISendWord( pinmask, counts_per_bit, str[i] );
}

// Currently unused
static inline void UPDISendBreak( int pinmask, int cpu_mhz )
{
	GPIO.out_w1tc = pinmask;
	GPIO.enable_w1ts = pinmask;
	esp_rom_delay_us(24600); // Table 32-3, attiny416 datashet, worst case scenario
	GPIO.out_w1ts = pinmask;
	 asm volatile( "nop" ); asm volatile( "nop" ); asm volatile( "nop" );// Tiny delay.
	GPIO.enable_w1tc = pinmask;
	esp_rom_delay_us(10);
}

static inline int UPDISyncStart( int pinmask, int cpu_mhz )
{
	DisableISR();
	GPIO.enable_w1tc = pinmask;
	GPIO.out_w1ts = pinmask;
	esp_rom_delay_us(1);
	int linebit = GPIO.in & pinmask;
	if( !linebit )
		return -1;

	GPIO.out_w1tc = pinmask;
	GPIO.enable_w1ts = pinmask;

	// 4.167ns per cycle, ~48 cycles, hold low for 200ns (only sync tip)
	DelayCycles( cpu_mhz/5 ); 

	// Release line
	GPIO.enable_w1tc = pinmask;
	GPIO.out_w1ts = pinmask;

	// Wait just a tiiiny bit.
	esp_rom_delay_us(1);

	// Make sure the AVR is still initializing after 2us.
	int ackbit = GPIO.in & pinmask;
	EnableISR();

	// 10-200 us later, the AVR releases the line.
	// (marker 2 on Figure 33-5 in ATTiny416 datasheet)
	esp_rom_delay_us(200);
	if( ackbit )
		return -2;
	return 0;	
}

int UPDIReceive( int pinmask, int clocksperbit )
{
    // Make sure it's in High-Z
	GPIO.enable_w1tc = pinmask;
	GPIO.out_w1ts = pinmask;
	DisableISR();

	int32_t lastNow = GetCCount();

	int32_t nextEvent = lastNow + (clocksperbit << 9);

	while( GPIO.in & pinmask )
	{
		lastNow = GetCCount();
		if ( lastNow > nextEvent )
		{
			EnableISR();
			return -4;
		}
	}

	// 1.5 bits in the future is when we want to sample.
	nextEvent = lastNow + clocksperbit + (clocksperbit>>1);

	int i;
	uint32_t word = 0;
	int parity = 0;
	for( i = 0; i < 10; i++ )
	{
		DelayUntil( nextEvent );
		nextEvent += clocksperbit;
		word >>= 1;
		if( GPIO.in & pinmask )
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

static inline void UPDIPowerOn( int pinmask, int pinmaskpower )
{
	GPIO.enable_w1tc = pinmask;     // High-Z the pinmask
	GPIO.enable_w1ts = pinmaskpower;
	GPIO.out_w1ts = pinmaskpower;
}

static inline void UPDIPowerOff( int pinmask, int pinmaskpower )
{
	GPIO.enable_w1tc = pinmask;     // Keep pinmask in high-z
	GPIO.enable_w1ts = pinmaskpower;
	GPIO.out_w1tc = pinmaskpower;
}


static int UPDITransact( uint32_t pinmask, uint32_t clocks_per_bit, uint8_t command, int expect_return, uint8_t * ret )
{

	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, command );

	int i;
	for( i = 0; i < expect_return; i++ )
	{
		int r = UPDIReceive( pinmask, clocks_per_bit );
		if( r < 0 )
		{
			return r;
		}
		ret[i] = r;
	}

	esp_rom_delay_us(100);

	return i;
}

static int UPDIReadMemoryByte( uint32_t pinmask, int clocks_per_bit, int address )
{
	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0x04 );
	UPDISendWord( pinmask, clocks_per_bit, (address)&0xff );
	UPDISendWord( pinmask, clocks_per_bit, (address>>8) );

	int r0 = UPDIReceive( pinmask, clocks_per_bit );
	esp_rom_delay_us(100);
	return r0;
}

static int UPDIWriteMemoryWord( uint32_t pinmask, int clocks_per_bit, int address, int word )
{
	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0x45 );
	UPDISendWord( pinmask, clocks_per_bit, (address)&0xff );
	UPDISendWord( pinmask, clocks_per_bit, (address>>8) );
	UPDISendWord( pinmask, clocks_per_bit, (word)&0xff );
	UPDISendWord( pinmask, clocks_per_bit, (word>>8) );

	int r0 = UPDIReceive( pinmask, clocks_per_bit );
	esp_rom_delay_us(100);
	return r0 != 0x40;
}

static int UPDIWriteMemoryByte( uint32_t pinmask, int clocks_per_bit, int address, int word )
{
	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0x44 );
	UPDISendWord( pinmask, clocks_per_bit, (address)&0xff );
	UPDISendWord( pinmask, clocks_per_bit, (address>>8) );
	UPDISendWord( pinmask, clocks_per_bit, (word)&0xff );

	int r0 = UPDIReceive( pinmask, clocks_per_bit );
	esp_rom_delay_us(100);
	return r0 != 0x40;
}

static int UPDISetup( uint32_t pinmask, int cpumhz, int baudrate, int * clocks_per_bit_ret, uint8_t sib[16] )
{
	int r;
	esp_rom_delay_us(10000);
	const int clocks_per_bit = ( cpumhz * 1000000 + (baudrate/2) ) / baudrate;
	r = UPDISyncStart( pinmask, cpumhz );
	if( r )
		return r;

	if( clocks_per_bit_ret )
		*clocks_per_bit_ret = clocks_per_bit;

	// Receive SIB

	return UPDITransact( pinmask, clocks_per_bit, 0xe5, 16, sib );
}

static int UPDIErase( uint32_t pinmask, int clocks_per_bit )
{
	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0xe0 );
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"esarEMVN", 8 ); //NVMErase", backwards.

	uint8_t csret = 0;
	int r = UPDITransact( pinmask, clocks_per_bit, 0x87, 1, &csret );  //LDCS, UPDI.ASI_KEY_STATUS -> Look for CHIPERASE

	if( r != 1 )
		return -12;

	if( ! (csret & 0x08) )
		return -13;

	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x59", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x59
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x00", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x00 Clear reset request.

	int retry;
	const int max_retry = 200;
	for( retry = 0; retry < max_retry; retry++ )
	{
		esp_rom_delay_us(10000);	
		r = UPDITransact( pinmask, clocks_per_bit, 0x8B, 1, &csret );  //LDCS, UPDI.ASI_SYS_STATUS -> Look for LOCKSTATUS
		if( r != 1 )
			return r;
		if( !(csret & 0x1) )
			break;
	}

	if( retry == max_retry )
		return -13;

	return 0;
}

static int UPDIFlash( uint32_t pinmask, int clocks_per_bit, uint8_t * program, int len, int flash_bootloader )
{
	int r;


	// First, check BOOTEND.
//	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x1280 /*FUSE*/ + 8 /* BOOTEND */
//	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x61 /*CLKCTRL MCLKCTRLB */ ); // WORKS!!!

// Doesn't help :(
//	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
//	UPDISendWord( pinmask, clocks_per_bit, 0xca ); //STCS 0x0a = 1 (ASI_SYS_CTRLA) = CLKREQ
//	UPDISendWord( pinmask, clocks_per_bit, 0x01 );

	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0xe0 );
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)" gorPMVN", 8 ); //NVMErase", backwards.


	uint8_t csret = 0;
	r = UPDITransact( pinmask, clocks_per_bit, 0x87, 1, &csret );  //LDCS, UPDI.ASI_KEY_STATUS -> Look for NVMPROG

	if( r != 1 )
		return -12;

	if( ! (csret & 0x10) ) // NVMPROG
		return -13;


	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x59", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x59
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x00", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x00 Clear reset request.

	int retry;
	const int max_retry = 200;
	for( retry = 0; retry < max_retry; retry++ )
	{
		r = UPDITransact( pinmask, clocks_per_bit, 0x8B, 1, &csret );  //LDCS, UPDI.ASI_SYS_STATUS -> Look for NVMPROG
		if( r != 1 )
			return r;
		if( (csret & 0x08) ) //NVMPROG
			break;
	}

	if( retry == max_retry )
		return -13;

	int memory_offset = 0x8000;

	r = UPDIWriteMemoryByte( pinmask, clocks_per_bit, 0x2000, 0xaa ); // Page Buffer Clear
	uprintf( "SWR: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x2000 );
	uprintf( "SRR: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x2000 );
	uprintf( "SRR: %02x\n", r );
	
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x8000 );
	uprintf( "RRRRF: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x8000 );
	uprintf( "RRRRF: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x8000 );
	uprintf( "RRRRF: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x1288 );
	uprintf( "RRRRR: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x1288 );
	uprintf( "RRRRR: %02x\n", r );

	if( !flash_bootloader )
	{
		r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x1280 /*FUSE*/ + 8 /* BOOTEND */ );
		if( r < 0 )
			return r;
		memory_offset += r<<8;
	}


	// Page size is 64 bytes.
	r = UPDIWriteMemoryWord( pinmask, clocks_per_bit, 0x1000 + 8 /*NVMCTRL.ADDR*/, 0x8000 );
	r = UPDIWriteMemoryByte( pinmask, clocks_per_bit, 0x1000 + 0 /*NVMCTRL.CTRLA*/, 0x04 ); // Page Buffer Clear
	uprintf( "WWWW: %02x\n", r );
	int bo;
	for( bo = 0; bo < 32; bo++ )
		r = UPDIWriteMemoryWord( pinmask, clocks_per_bit, 0x8000 + bo*2, 0xaa55 );
	uprintf( "WWWW: %02x\n", r );
	r = UPDIWriteMemoryByte( pinmask, clocks_per_bit, 0x1000 + 0 /*NVMCTRL.CTRLA*/, 0x03 ); // Write page

	int to_max = 10;
	int to;
	for( to = 0; to < to_max; to++ )
	{
		r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x1000 + 2 );
		if( r & 0x40 ) continue; // Invalid Read
		if( r & 0x01 ) continue; // FBUSY
		break;
	}

	uprintf( "TO : %d\n", to );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x8000 );
	uprintf( "RRRRR: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x8000 );
	uprintf( "RRRRR: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x8000 );
	uprintf( "RRRRR: %02x\n", r );
	r = UPDIReadMemoryByte( pinmask, clocks_per_bit, 0x2000 );
	uprintf( "SRR: %02x\n", r );

	// Reset chip.
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x59", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x59
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x00", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x00 Clear reset request.


	esp_rom_delay_us(5000);

	return 0;
}

static int UPDIActivateUserRow( uint32_t pinmask, int clocks_per_bit )
{
	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0xe0 );
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"NVMUs&te", 8 );
	return 0;
}






#if 0
// This is a hacky thing, but if you are laaaaazzzyyyy and don't want to add a 10k
// resistor, youcan do this.  It glitches the line high very, very briefly.  
// Enable for when you don't have a 10k pull-upand are relying on the internal pull-up.
// WARNING: If you set this, you should set the drive current to 5mA.
#define R_GLITCH_HIGH

// You should interface to this file via these functions

static int DoSongAndDanceToEnterPgmMode(int t1coeff, int pinmask);
static void SendWord32( int t1coeff, int pinmask, uint8_t command, uint32_t value );
static int ReadWord32( int t1coeff, int pinmask, uint8_t command, uint32_t * value );

#define CDMCONTROL  0x10
#define CDMSTATUS   0x11
#define CDATA0      0x4
#define CDATA1      0x5
#define CHARTINFO   0x12
#define CABSTRACTCS 0x16
#define CCOMMAND    0x17
#define CABSTRACTAUTO 0x18
#define CPROGBUF0   0x20
#define CHALTSUM0   0x40

// TODO: Add continuation (bypass) functions.
// TODO: Consider adding parity bit (though it seems rather useless)


// All three bit functions assume bus state will be in
// 	GPIO.out_w1ts = pinmask;
//	GPIO.enable_w1ts = pinmask;
// when they are called.
static inline void Send1Bit( int t1coeff, int pinmask )
{
	int i;

	// Low for a nominal period of time.
	// High for a nominal period of time.

	GPIO.out_w1tc = pinmask;
	for( i = 1; i < t1coeff; i++ ) asm volatile( "nop" );
	GPIO.out_w1ts = pinmask;
	for( i = 1; i < t1coeff; i++ ) asm volatile( "nop" );
}


static inline void Send0Bit( int t1coeff, int pinmask )
{
	// Low for a LONG period of time.
	// High for a nominal period of time.

	int i;
	int longwait = t1coeff*4;
	GPIO.out_w1tc = pinmask;
	for( i = 1; i < longwait; i++ ) asm volatile( "nop" );
	GPIO.out_w1ts = pinmask;
	for( i = 1; i < t1coeff; i++ ) asm volatile( "nop" );
}

// returns 0 if 0
// returns 1 if 1
// returns 2 if timeout.
static inline int ReadBit( int t1coeff, int pinmask )
{
	// Drive low, very briefly.  Let drift high.
	// See if CH32V003 is holding low.

	int timeout = 0;
	int ret = 0;
	int i;
	int medwait = t1coeff * 2;
	GPIO.out_w1tc = pinmask;
	for( i = 1; i < t1coeff; i++ ) asm volatile( "nop" );
	GPIO.enable_w1tc = pinmask;
	GPIO.out_w1ts = pinmask;
#ifdef R_GLITCH_HIGH
	int halfwait = t1coeff / 2;
	for( i = 1; i < halfwait; i++ ) asm volatile( "nop" );
	GPIO.enable_w1ts = pinmask;
	GPIO.enable_w1tc = pinmask;
	for( i = 1; i < halfwait; i++ ) asm volatile( "nop" );
#else
	for( i = 1; i < medwait; i++ ) asm volatile( "nop" );
#endif
	ret = GPIO.in;

#ifdef R_GLITCH_HIGH
	if( !(ret & pinmask) )
	{
		// Wait if still low.
		for( i = 1; i < medwait; i++ ) asm volatile( "nop" );
		GPIO.enable_w1ts = pinmask;
		GPIO.enable_w1tc = pinmask;
	}
#endif
	for( timeout = 0; timeout < MAX_IN_TIMEOUT; timeout++ )
	{
		if( GPIO.in & pinmask )
		{
			GPIO.enable_w1ts = pinmask;
			int fastwait = t1coeff / 2;
			for( i = 1; i < fastwait; i++ ) asm volatile( "nop" );
			return !!(ret & pinmask);
		}
	}
	
	// Force high anyway so, though hazarded, we can still move along.
	GPIO.enable_w1ts = pinmask;
	return 2;
}

static void SendWord32( int t1coeff, int pinmask, uint8_t command, uint32_t value )
{
 	GPIO.out_w1ts = pinmask;
	GPIO.enable_w1ts = pinmask;

	DisableISR();
	Send1Bit( t1coeff, pinmask );
	uint32_t mask;
	for( mask = 1<<6; mask; mask >>= 1 )
	{
		if( command & mask )
			Send1Bit(t1coeff, pinmask);
		else
			Send0Bit(t1coeff, pinmask);
	}
	Send1Bit( t1coeff, pinmask );
	for( mask = 1<<31; mask; mask >>= 1 )
	{
		if( value & mask )
			Send1Bit(t1coeff, pinmask);
		else
			Send0Bit(t1coeff, pinmask);
	}
	EnableISR();
	esp_rom_delay_us(5); // Sometimes 2 is too short.
}

// returns 0 if no error, otherwise error.
static int ReadWord32( int t1coeff, int pinmask, uint8_t command, uint32_t * value )
{
 	GPIO.out_w1ts = pinmask;
	GPIO.enable_w1ts = pinmask;

	DisableISR();
	Send1Bit( t1coeff, pinmask );
	int i;
	uint32_t mask;
	for( mask = 1<<6; mask; mask >>= 1 )
	{
		if( command & mask )
			Send1Bit(t1coeff, pinmask);
		else
			Send0Bit(t1coeff, pinmask);
	}
	Send0Bit( t1coeff, pinmask );
	uint32_t rval = 0;
	for( i = 0; i < 32; i++ )
	{
		rval <<= 1;
		int r = ReadBit( t1coeff, pinmask );
		if( r == 1 )
			rval |= 1;
		if( r == 2 )
		{
			EnableISR();
			return -1;
		}
	}
	*value = rval;
	EnableISR();
	esp_rom_delay_us(5); // Sometimes 2 is too short.
	return 0;
}

static inline void ExecuteTimePairs(int t1coeff, int pinmask, const uint16_t * pairs, int numpairs, int iterations )
{
	int i, j, k;
	for( k = 0; k < iterations; k++ )
	{
		const uint16_t * tp = pairs;
		for( j = 0; j < numpairs; j++ )
		{
			int t1v = t1coeff * (*(tp++))-1;
			GPIO.out_w1tc = pinmask;
			for( i = 0; i < t1v; i++ ) asm volatile( "nop" );
			GPIO.out_w1ts = pinmask;
			t1v = t1coeff * (*(tp++))-1;
			for( i = 0; i < t1v; i++ ) asm volatile( "nop" );
		}
	}
}

// Returns 0 if chips is present
// Returns 1 if chip is not present
// Returns 2 if there was a bus fault.
static int DoSongAndDanceToEnterPgmMode(int t1coeff, int pinmask)
{
	// XXX UNTESTED UNTESTED!!!!

	static const uint16_t timepairs1[] ={
		32, 12, //  8.2us / 3.1us
		36, 12, //  9.2us / 3.1us
		392, 366 // 102.3us / 95.3us
	};

	// Repeat for 199x
	static const uint16_t timepairs2[] ={
		15, 12, //  4.1us / 3.1us
		32, 12, //  8.2us / 3.1us
		36, 12, //  9.3us / 3.1us
		392, 366 // 102.3us / 95.3us
	};

	// Repeat for 10x
	static const uint16_t timepairs3[] ={
		15, 807, //  4.1us / 210us
		24, 8,   //  6.3us / 2us
		32, 8,   //  8.4us / 2us
		24, 10,  //  6.2us / 2.4us
		20, 8,   //  5.2us / 2.1us
		239, 8,  //  62.3us / 2.1us
		32, 20,  //  8.4us / 5.3us
		8, 32,   //  2.2us / 8.4us
		24, 8,   //  6.3us / 2.1us
		32, 8,   //  8.4us / 2.1us
		26, 7,   //  6.9us / 1.7us
		20, 8,   //  5.2us / 2.1us
		239, 8,  //  62.3us / 2.1us
		32, 20,  //  8.4us / 5.3us
		8, 22,   //  2us / 5.3us
		24, 8,   //  6.3us 2.1us
		24, 8,   //  6.3us 2.1us
		31, 6,   //  8us / 1.6us
		25, 307, // 6.6us / 80us
	};

	DisableISR();

	ExecuteTimePairs( t1coeff, pinmask, timepairs1, 3, 1 );
	ExecuteTimePairs( t1coeff, pinmask, timepairs2, 4, 199 );
	ExecuteTimePairs( t1coeff, pinmask, timepairs3, 19, 10 );

	// THIS IS WRONG!!!! THIS IS NOT A PRESENT BIT. 
	int present = ReadBit( t1coeff * 8, pinmask );
	GPIO.enable_w1ts = pinmask;
	GPIO.out_w1tc = pinmask;
	esp_rom_delay_us( 2000 );
	GPIO.out_w1ts = pinmask;
	EnableISR();
	esp_rom_delay_us( 1 );

	return present;
}

#endif

#endif

