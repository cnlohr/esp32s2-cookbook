// CH32V003 SWIO minimal reference implementation for bit banged IO
// on the ESP32-S2.
//
// Copyright 2023 Charles Lohr, May be licensed under the MIT/x11 or NewBSD
// licenses.  You choose.  (Can be included in commercial and copyleft work)
//
// This file is original work.
//
// Mostly tested, though, not perfect.  Expect to tweak some things.
// DoSongAndDanceToEnterPgmMode is almost completely untested.
// This is the weird song-and-dance that the WCH LinkE does when
// connecting to a CH32V003 part with unknown state.  This is probably
// incorrect, but isn't really needed unless things get really cursed.

#ifndef _CH32V003_SWIO_H
#define _CH32V003_SWIO_H

// This is a hacky thing, but if you are laaaaazzzyyyy and don't want to add a 10k
// resistor, youcan do this.  It glitches the line high very, very briefly.  
// Enable for when you don't have a 10k pull-upand are relying on the internal pull-up.
// WARNING: If you set this, you should set the drive current to 5mA.
#define R_GLITCH_HIGH

// You should interface to this file via these functions

struct SWIOState
{
	// Set these before calling any functions
	int t1coeff;
	int pinmask;

	// Zero the rest of the structure.
	uint32_t statetag;
	uint32_t lastwriteflags;
	uint32_t currentstateval;
};

#define STTAG( x ) (*((uint32_t*)(x)))

#define IRAM IRAM_ATTR

static int DoSongAndDanceToEnterPgmMode( struct SWIOState * state );
static void WriteReg32( struct SWIOState * state, uint8_t command, uint32_t value ) IRAM;
static int ReadReg32( struct SWIOState * state, uint8_t command, uint32_t * value ) IRAM;
static int ReadWord( struct SWIOState * state, uint32_t word, uint32_t * ret );
static int WriteWord( struct SWIOState * state, uint32_t word, uint32_t val );
static int WaitForFlash( struct SWIOState * state );
static int WaitForDoneOp( struct SWIOState * state );


#define DMDATA0        0x04
#define DMDATA1        0x05
#define DMCONTROL      0x10
#define DMSTATUS       0x11
#define DMHARTINFO     0x12
#define DMABSTRACTCS   0x16
#define DMCOMMAND      0x17
#define DMABSTRACTAUTO 0x18
#define DMPROGBUF0     0x20
#define DMPROGBUF1     0x21
#define DMPROGBUF2     0x22
#define DMPROGBUF3     0x23
#define DMPROGBUF4     0x24
#define DMPROGBUF5     0x25
#define DMPROGBUF6     0x26
#define DMPROGBUF7     0x27

#define DMCPBR       0x7C
#define DMCFGR       0x7D
#define DMSHDWCFGR   0x7E



#define FLASH_STATR_WRPRTERR                    ((uint8_t)0x10) 
#define CR_PAGE_PG                 ((uint32_t)0x00010000)
#define CR_BUF_LOAD                ((uint32_t)0x00040000)

static inline void Send1Bit( int t1coeff, int pinmask ) IRAM;
static inline void Send0Bit( int t1coeff, int pinmask ) IRAM;
static inline int ReadBit( struct SWIOState * state ) IRAM;

static inline void PrecDelay( int delay )
{
	asm volatile( 
"1:	addi %[delay], %[delay], -1\n"
"	bbci %[delay], 31, 1b\n" : [delay]"+r"(delay)  );
}

// TODO: Add continuation (bypass) functions.
// TODO: Consider adding parity bit (though it seems rather useless)

// All three bit functions assume bus state will be in
// 	GPIO.out_w1ts = pinmask;
//	GPIO.enable_w1ts = pinmask;
// when they are called.
static inline void Send1Bit( int t1coeff, int pinmask )
{
	// Low for a nominal period of time.
	// High for a nominal period of time.

	GPIO.out_w1tc = pinmask;
	PrecDelay( t1coeff );
	GPIO.out_w1ts = pinmask;
	PrecDelay( t1coeff );
}

static inline void Send0Bit( int t1coeff, int pinmask )
{
	// Low for a LONG period of time.
	// High for a nominal period of time.
	int longwait = t1coeff*4;
	GPIO.out_w1tc = pinmask;
	PrecDelay( longwait );
	GPIO.out_w1ts = pinmask;
	PrecDelay( t1coeff );
}

// returns 0 if 0
// returns 1 if 1
// returns 2 if timeout.
static inline int ReadBit( struct SWIOState * state )
{
	int t1coeff = state->t1coeff;
	int pinmask = state->pinmask;

	// Drive low, very briefly.  Let drift high.
	// See if CH32V003 is holding low.

	int timeout = 0;
	int ret = 0;
	int medwait = t1coeff * 2;
	GPIO.out_w1tc = pinmask;
	PrecDelay( t1coeff );
	GPIO.enable_w1tc = pinmask;
	GPIO.out_w1ts = pinmask;
#ifdef R_GLITCH_HIGH
	int halfwait = t1coeff / 2;
	PrecDelay( halfwait );
	GPIO.enable_w1ts = pinmask;
	GPIO.enable_w1tc = pinmask;
	PrecDelay( halfwait );
#else
	PrecDelay( medwait );
#endif
	ret = GPIO.in;

#ifdef R_GLITCH_HIGH
	if( !(ret & pinmask) )
	{
		// Wait if still low.
		PrecDelay( medwait );
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
			PrecDelay( fastwait );
			return !!(ret & pinmask);
		}
	}
	
	// Force high anyway so, though hazarded, we can still move along.
	GPIO.enable_w1ts = pinmask;
	return 2;
}

static void WriteReg32( struct SWIOState * state, uint8_t command, uint32_t value )
{
	int t1coeff = state->t1coeff;
	int pinmask = state->pinmask;

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
	esp_rom_delay_us(8); // Sometimes 2 is too short.
}

// returns 0 if no error, otherwise error.
static int ReadReg32( struct SWIOState * state, uint8_t command, uint32_t * value )
{
	int t1coeff = state->t1coeff;
	int pinmask = state->pinmask;

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
		int r = ReadBit( state );
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
	esp_rom_delay_us(8); // Sometimes 2 is too short.
	return 0;
}

static inline void ExecuteTimePairs( struct SWIOState * state, const uint16_t * pairs, int numpairs, int iterations )
{
	int t1coeff = state->t1coeff;
	int pinmask = state->pinmask;

	int j, k;
	for( k = 0; k < iterations; k++ )
	{
		const uint16_t * tp = pairs;
		for( j = 0; j < numpairs; j++ )
		{
			int t1v = t1coeff * (*(tp++))-1;
			GPIO.out_w1tc = pinmask;
			PrecDelay( t1v );
			GPIO.out_w1ts = pinmask;
			t1v = t1coeff * (*(tp++))-1;
			PrecDelay( t1v );
		}
	}
}

// Returns 0 if chips is present
// Returns 1 if chip is not present
// Returns 2 if there was a bus fault.
static int DoSongAndDanceToEnterPgmMode( struct SWIOState * state )
{
	int pinmask = state->pinmask;
	// XXX MOSTLY UNTESTED!!!!

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

	ExecuteTimePairs( state, timepairs1, 3, 1 );
	ExecuteTimePairs( state, timepairs2, 4, 199 );
	ExecuteTimePairs( state, timepairs3, 19, 10 );

	// THIS IS WRONG!!!! THIS IS NOT A PRESENT BIT. 
	int present = ReadBit( state ); // Actually here t1coeff, for this should be *= 8!
	GPIO.enable_w1ts = pinmask;
	GPIO.out_w1tc = pinmask;
	esp_rom_delay_us( 2000 );
	GPIO.out_w1ts = pinmask;
	EnableISR();
	esp_rom_delay_us( 1 );

	return present;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Higher level functions

static int WaitForFlash( struct SWIOState * iss )
{
	struct SWIOState * dev = iss;
	uint32_t rw, timeout = 0;
	do
	{
		rw = 0;
		ReadWord( dev, 0x4002200C, &rw ); // FLASH_STATR => 0x4002200C
		if( timeout++ > 100 ) return -1;
	} while(rw & 1);  // BSY flag.

	if( rw & FLASH_STATR_WRPRTERR )
	{
		return -44;
	}
	return 0;
}

static int WaitForDoneOp( struct SWIOState * iss )
{
	int r;
	uint32_t rrv;
	int ret = 0;
	struct SWIOState * dev = iss;
	do
	{
		r = ReadReg32( dev, DMABSTRACTCS, &rrv );
		if( r ) return r;
	}
	while( rrv & (1<<12) );
	if( (rrv >> 8 ) & 7 )
	{
		WriteReg32( dev, DMABSTRACTCS, 0x00000700 );
		ret = -33;
	}
	return ret;
}

static int ReadWord( struct SWIOState * iss, uint32_t address_to_read, uint32_t * data )
{
	struct SWIOState * dev = iss;
	int ret = 0;

	if( iss->statetag != STTAG( "RDSQ" ) || address_to_read != iss->currentstateval )
	{
		if( iss->statetag != STTAG( "RDSQ" ) )
		{
			WriteReg32( dev, DMABSTRACTAUTO, 0 ); // Disable Autoexec.
			WriteReg32( dev, DMPROGBUF0, 0x00052283 ); // lw x5,0(x10)
			WriteReg32( dev, DMPROGBUF1, 0x0002a303 ); // lw x6,0(x5)
			WriteReg32( dev, DMPROGBUF2, 0x00428293 ); // addi x5, x5, 4
			WriteReg32( dev, DMPROGBUF3, 0x0065a023 ); // sw x6,0(x11) // Write back to DATA0
			WriteReg32( dev, DMPROGBUF4, 0x00552023 ); // sw x5,0(x10) // Write addy to DATA1
			WriteReg32( dev, DMPROGBUF5, 0x00100073 ); // ebreak

			WriteReg32( dev, DMDATA0, 0xe00000f4 );   // DATA0's location in memory.
			WriteReg32( dev, DMCOMMAND, 0x0023100b ); // Copy data to x11
			WriteReg32( dev, DMDATA0, 0xe00000f8 );   // DATA1's location in memory.
			WriteReg32( dev, DMCOMMAND, 0x0023100a ); // Copy data to x10
			WriteReg32( dev, DMABSTRACTAUTO, 1 ); // Enable Autoexec.
		}

		WriteReg32( dev, DMDATA1, address_to_read );
		WriteReg32( dev, DMCOMMAND, 0x00241000 ); // Only execute.

		WaitForDoneOp( dev );
		iss->statetag = STTAG( "RDSQ" );
		ret |= iss->currentstateval = address_to_read;
	}

	iss->currentstateval += 4;

	return ret | ReadReg32( dev, DMDATA0, data );

}

static int WriteWord( struct SWIOState * iss, uint32_t address_to_write, uint32_t data )
{
	// Synonyms here.
	struct SWIOState * dev = iss;

	int ret = 0;

	int is_flash = 0;
	if( ( address_to_write & 0xff000000 ) == 0x08000000 || ( address_to_write & 0x1FFFF800 ) == 0x1FFFF000 )
	{
		// Is flash.
		is_flash = 1;
	}

	if( iss->statetag != STTAG( "WRSQ" ) || is_flash != iss->lastwriteflags )
	{
		WriteReg32( dev, DMABSTRACTAUTO, 0x00000000 ); // Disable Autoexec.

		// Different address, so we don't need to re-write all the program regs.
		WriteReg32( dev, DMPROGBUF0, 0x00032283 ); // lw x5,0(x6)
		WriteReg32( dev, DMPROGBUF1, 0x0072a023 ); // sw x7,0(x5)
		WriteReg32( dev, DMPROGBUF2, 0x00428293 ); // addi x5, x5, 4
		WriteReg32( dev, DMPROGBUF3, 0x00532023 ); // sw x5,0(x6)
		if( is_flash )
		{
			// After writing to memory, also hit up page load flag.
			WriteReg32( dev, DMPROGBUF4, 0x00942023 ); // sw x9,0(x8)
			WriteReg32( dev, DMPROGBUF5, 0x00100073 ); // ebreak

			WriteReg32( dev, DMDATA0, 0x40022010 ); // (intptr_t)&FLASH->CTLR
			WriteReg32( dev, DMCOMMAND, 0x00231008 ); // Copy data to x8
			WriteReg32( dev, DMDATA0, CR_PAGE_PG|CR_BUF_LOAD);
			WriteReg32( dev, DMCOMMAND, 0x00231009 ); // Copy data to x9
		}
		else
		{
			WriteReg32( dev, DMPROGBUF4, 0x00100073 ); // ebreak
		}


		WriteReg32( dev, DMDATA0, 0xe00000f8); // Address of DATA1.
		WriteReg32( dev, DMCOMMAND, 0x00231006 ); // Location of DATA1 to x6

		iss->lastwriteflags = is_flash;

		WriteReg32( dev, DMDATA1, address_to_write );

		iss->statetag = STTAG( "WRSQ" );
		iss->currentstateval = address_to_write;

		WriteReg32( dev, DMDATA0, data );
		WriteReg32( dev, DMCOMMAND, 0x00271007 ); // Copy data to x7, and execute program.
		WriteReg32( dev, DMABSTRACTAUTO, 1 ); // Enable Autoexec.

		if( WaitForDoneOp( dev ) ) return -3;
	}
	else
	{
		if( address_to_write != iss->currentstateval )
		{
			WriteReg32( dev, DMABSTRACTAUTO, 0 ); // Disable Autoexec.
			WriteReg32( dev, DMDATA1, address_to_write );
			WriteReg32( dev, DMABSTRACTAUTO, 1 ); // Enable Autoexec.
		}
		WriteReg32( dev, DMDATA0, data );
		if( WaitForDoneOp( dev ) ) return -4;
	}

	iss->currentstateval += 4;
	return ret;
}


#endif

