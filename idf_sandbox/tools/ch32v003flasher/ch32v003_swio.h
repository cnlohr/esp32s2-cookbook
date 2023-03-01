// XXX UNTESTED XXX PLEASE DO NOT TRY OUT.

#ifndef _CH32V003_DEBUGGER_H
#define _CH32V003_DEBUGGER_H

static int DoSongAndDanceToEnterPgmMode(int t1coeff, int pinmask);
static void SendWord32( int t1coeff, int pinmask, uint8_t command, uint32_t value );
static int ReadWord32( int t1coeff, int pinmask, uint8_t command, uint32_t * value );

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
	for( i = 1; i < medwait; i++ ) asm volatile( "nop" );
	ret = GPIO.in;
	for( timeout = 0; timeout < 10; timeout++ )
	{
		for( i = 1; i < t1coeff; i++ ) asm volatile( "nop" );
		if( GPIO.in & pinmask )
		{
			GPIO.enable_w1ts = pinmask;
			int fastwait = t1coeff / 2;
			for( i = 1; i < fastwait; i++ ) asm volatile( "nop" );
			return !!(ret & pinmask);
		}
	}
	return 2;
}

static void SendWord32( int t1coeff, int pinmask, uint8_t command, uint32_t value )
{
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
	esp_rom_delay_us(5);
}

// returns 0 if no error, otherwise error.
static int ReadWord32( int t1coeff, int pinmask, uint8_t command, uint32_t * value )
{
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
			return -1;
	}
	*value = rval;
	EnableISR();
	esp_rom_delay_us(5);
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

