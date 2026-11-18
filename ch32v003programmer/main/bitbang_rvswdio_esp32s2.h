#ifndef BITBANG_RVSWDIO_ESP32S2_H
#define BITBANG_RVSWDIO_ESP32S2_H

#define SWDIO_T1 5
#define SWDIO_T2 10
#define SWDIO_T3 20
#define SWDIO_T4 40
#define SWD_T1 10
#define SWD_T2 17 // Used for CH591/2 and CH571/3
#define SWD_T3 35 // Used for init and for CH32V103
#define SWD_T4 12 // Start and stop pulse. Should be 250ns

static inline void PrecDelay( int delay )
{
	asm volatile( 
	"1: addi %[delay], %[delay], -1\n"
	"   bbci %[delay], 31, 1b\n" : [delay]"+r"(delay) );
}

// TODO: Add continuation (bypass) functions.
// TODO: Consider adding parity bit (though it seems rather useless)

// All three bit functions assume bus state will be in
// GPIO.out_w1ts = pinmask;
// GPIO.enable_w1ts = pinmask;
// when they are called.
static inline void Send1BitSWIO( struct SWIOState * state )
{
	// Low for a nominal period of time.
	// High for a nominal period of time.

	GPIO.out_w1tc = state->pinmaskD;
	PrecDelay( SWDIO_T2 );
	GPIO.out_w1ts = state->pinmaskD;
	PrecDelay( SWDIO_T2 );
}

static inline void Send0BitSWIO( struct SWIOState * state )
{
	// Low for a LONG period of time.
	// High for a nominal period of time.
	// int longwait = state->t1coeff*4;
	GPIO.out_w1tc = state->pinmaskD;
	PrecDelay( SWDIO_T4 );
	GPIO.out_w1ts = state->pinmaskD;
	PrecDelay( SWDIO_T2 );
}

// returns 0 if 0
// returns 1 if 1
// returns 2 if timeout.
static inline int ReadBitSWIO( struct SWIOState * state )
{
	// int t1coeff = state->t1coeff;
	int pinmaskD = state->pinmaskD;

	// Drive low, very briefly.  Let drift high.
	// See if CH32V003 is holding low.

	int timeout = 0;
	int ret = 0;
	// int medwait = t1coeff * 2;
	GPIO.out_w1tc = pinmaskD;
	PrecDelay( SWDIO_T2 );
	GPIO.enable_w1tc = pinmaskD;
	GPIO.out_w1ts = pinmaskD;
#ifdef R_GLITCH_HIGH
	// int halfwait = t1coeff / 2;
	PrecDelay( SWDIO_T1 );
	GPIO.enable_w1ts = pinmaskD;
	GPIO.enable_w1tc = pinmaskD;
	PrecDelay( SWDIO_T1 );
#else
	PrecDelay( SWDIO_T3 );
#endif
	ret = GPIO.in;

#ifdef R_GLITCH_HIGH
	if( !(ret & pinmaskD) )
	{
		// Wait if still low.
		PrecDelay( SWDIO_T3 );
		GPIO.enable_w1ts = pinmaskD;
		GPIO.enable_w1tc = pinmaskD;
	}
#endif
	for( timeout = 0; timeout < MAX_IN_TIMEOUT; timeout++ )
	{
		if( GPIO.in & pinmaskD )
		{
			GPIO.enable_w1ts = pinmaskD;
			// int fastwait = t1coeff / 2;
			PrecDelay( SWDIO_T1 );
			return !!(ret & pinmaskD);
		}
	}
	
	// Force high anyway so, though hazarded, we can still move along.
	GPIO.enable_w1ts = pinmaskD;
	return 2;
}


static inline void SendBitRVSWD( struct SWIOState * state, int val )
{
  int t1coeff = state->t1coeff;
	int pinmaskC = state->pinmaskC;
	int pinmaskD = state->pinmaskD;
	// Assume:
	// SWD is in indeterminte state.
	// SWC is HIGH
	GPIO.out_w1tc = pinmaskC;
	if( val )
		GPIO.out_w1ts = pinmaskD;
	else
		GPIO.out_w1tc = pinmaskD;
	GPIO.enable_w1ts = pinmaskD;
	PrecDelay( t1coeff );
	GPIO.out_w1ts = pinmaskC;
	PrecDelay( t1coeff );
}

static inline int ReadBitRVSWD( struct SWIOState * state )
{
  int t1coeff = state->t1coeff;
	int pinmaskC = state->pinmaskC;
	int pinmaskD = state->pinmaskD;
  
	GPIO.enable_w1tc = pinmaskD;
	GPIO.out_w1tc = pinmaskC;
	GPIO.out_w1ts = pinmaskD;
	PrecDelay( t1coeff );
	int r = !!(GPIO.in & pinmaskD);
	GPIO.out_w1ts = pinmaskC;
	PrecDelay( t1coeff );
	return r;
}

static void SWDStop( struct SWIOState * state )
{
	int pinmaskC = state->pinmaskC;
	int pinmaskD = state->pinmaskD;

	GPIO.out_w1tc = pinmaskC;
	PrecDelay( SWD_T4 );
	GPIO.out_w1tc = pinmaskD;
	GPIO.enable_w1ts = pinmaskD;
	PrecDelay( SWD_T4 );
	GPIO.out_w1ts = pinmaskC;
	PrecDelay( SWD_T4 );
	GPIO.out_w1ts = pinmaskD;

	esp_rom_delay_us(2); // Sometimes 2 is too short.
	EnableISR();
}

static void RVFinishRegop( struct SWIOState * state )
{
	ReadBitRVSWD( state ); // ???
	ReadBitRVSWD( state ); // ???
	ReadBitRVSWD( state ); // ???
	SendBitRVSWD( state, 1 ); // 0 for register, 1 for value
	SendBitRVSWD( state, 0 ); // ??? Seems to have somstate

	SWDStop( state );
}

#if defined(OLD_SWD_SUPPORT) && OLD_SWD_SUPPORT
static void SWDWakeUP( struct SWIOState * state )
{
	int t1coeff = state->t1coeff;
	int pinmaskC = state->pinmaskC;

	DisableISR();
	PrecDelay( SWD_T4 );
	for (int i = 0; i < 100; i++) {
		GPIO.out_w1tc = pinmaskC;
		// funDigitalWriteMask( PIN_SWC_MASK, 0 );
		PrecDelay( t1coeff );
		// funDigitalWriteMask( PIN_SWC_MASK, 1 );
		GPIO.out_w1ts = pinmaskC;
		PrecDelay( t1coeff );
	}

	SWDStop( state );
}

static void SWDLongOP( struct SWIOState * state, uint8_t reg, uint32_t * value, uint8_t op )
{
	uint32_t mask;
	// Start
	// funDigitalWriteMask( PIN_SWD_MASK, 0 );
	GPIO.out_w1tc = state->pinmaskD;
	PrecDelay( SWD_T4 );

	for( mask = 1<<6; mask; mask >>= 1 )
	{
		int v = !!(reg & mask);
		SendBitRVSWD( state, v );
	}
	for( mask = 1<<31; mask; mask >>= 1 )
	{
		int v = !!(*value & mask);
		SendBitRVSWD( state, v );
	}

	SendBitRVSWD( state, op&2 );
	SendBitRVSWD( state, op&1 );
	// SendBitRVSWD( state, 0 ); // Parity is always 0
	ReadBitRVSWD( state );

	for( int i = 0; i < 7; i++ )
	{
		ReadBitRVSWD( state );
	}

	uint32_t rval = 0;
	for( int i = 0; i < 32; i++ )
	{
		rval <<= 1;
		int r = ReadBitRVSWD( state );
		if( r == 1 ) rval |= 1;
	}

	ReadBitRVSWD( state );
	ReadBitRVSWD( state );

	if ( op == 2 ) ReadBitRVSWD( state );
	else SendBitRVSWD( state, 1 );

	SWDStop( state );

	if ( op == 1 ) memcpy( value, &rval, 4 );
}
#endif

static void MCFWriteReg32( struct SWIOState * state, uint8_t command, uint32_t value )
{
	int pinmaskD = state->pinmaskD;
	int pinmaskC = state->pinmaskC;

	GPIO.out_w1ts = pinmaskC;
	GPIO.enable_w1ts = pinmaskC;
	GPIO.out_w1ts = pinmaskD;
	GPIO.enable_w1ts = pinmaskD;
	// FUN_PRINTF_DEBUG( "CO: (%08x=>%08x) %08x %08x %d %d\n", command, value, pinmaskD, pinmaskC, t1coeff, state->opmode );
	if( state->opmode == 1 )
	{
		DisableISR();
		Send1BitSWIO( state );
		uint32_t mask;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			if( command & mask )
				Send1BitSWIO( state );
			else
				Send0BitSWIO( state );
		}
		Send1BitSWIO( state );
		for( mask = 1<<31; mask; mask >>= 1 )
		{
			if( value & mask )
				Send1BitSWIO( state );
			else
				Send0BitSWIO( state );
		}
		EnableISR();
		esp_rom_delay_us(8); // Sometimes 2 is too short.
	}
	else if( state->opmode == 2 )
	{
		DisableISR();
		uint32_t mask;
		GPIO.out_w1tc = pinmaskD;
		PrecDelay( SWD_T4 );
		int parity = 1;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			int v = !!(command & mask);
			parity ^= v;
			SendBitRVSWD( state, v );
		}
		SendBitRVSWD( state, 1 ); // Write = Set high
		SendBitRVSWD( state, parity );
		ReadBitRVSWD( state ); // ???
		ReadBitRVSWD( state ); // Seems only need to be set for first transaction (We are ignoring that though)
		ReadBitRVSWD( state ); // ???
		SendBitRVSWD( state, 0 ); // 0 for register, 1 for value.
		SendBitRVSWD( state, 0 ); // ???  Seems to have something to do with halting.

		parity = 0;
		for( mask = 1<<31; mask; mask >>= 1 )
		{
			int v = !!(value & mask);
			parity ^= v;
			SendBitRVSWD( state, v );
		}
		SendBitRVSWD( state, parity );

		RVFinishRegop( state );
	}
#if defined(OLD_SWD_SUPPORT) && OLD_SWD_SUPPORT
	else if( state->opmode == 3 )
	{
		uint32_t data = value;
		SWDLongOP( state, command, &data, 2 );
	}
#endif
}

// returns 0 if no error, otherwise error.
static int MCFReadReg32( struct SWIOState * state, uint8_t command, uint32_t * value )
{
	int pinmaskD = state->pinmaskD;
	int pinmaskC = state->pinmaskC;

	GPIO.out_w1ts = pinmaskC;
	GPIO.enable_w1ts = pinmaskC;
	GPIO.out_w1ts = pinmaskD;
	GPIO.enable_w1ts = pinmaskD;

	if( state->opmode == 1 )
	{
		DisableISR();
		Send1BitSWIO( state );
		int i;
		uint32_t mask;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			if( command & mask )
				Send1BitSWIO( state );
			else
				Send0BitSWIO( state );
		}
		Send0BitSWIO( state );
		uint32_t rval = 0;
		for( i = 0; i < 32; i++ )
		{
			rval <<= 1;
			int r = ReadBitSWIO( state );
			if( r == 1 )
				rval |= 1;
			if( r == 2 )
			{
				EnableISR();
				return -21;
			}
		}
		*value = rval;
		EnableISR();
		esp_rom_delay_us(8); // Sometimes 2 is too short.
	}
	else if( state->opmode == 2 )
	{
		DisableISR();
		int mask;
		GPIO.out_w1tc = pinmaskD;
		PrecDelay( SWD_T4 );
		int parity = 0;
		for( mask = 1<<6; mask; mask >>= 1 )
		{
			int v = !!(command & mask);
			parity ^= v;
			SendBitRVSWD( state, v );
		}
		SendBitRVSWD( state, 0 ); // Read = Set low
		SendBitRVSWD( state, parity );
		ReadBitRVSWD( state ); // ???
		ReadBitRVSWD( state ); // ???
		ReadBitRVSWD( state ); // ???
		SendBitRVSWD( state, 0 ); // 0 for register, 1 for value
		SendBitRVSWD( state, 0 ); // ??? Seems to have something to do with halting?

		uint32_t rval = 0;
		int i;
		parity = 0;
		for( i = 0; i < 32; i++ )
		{
			rval <<= 1;
			int r = ReadBitRVSWD( state );
			if( r == 1 )
			{
				rval |= 1;
				parity ^= 1;
			}
			if( r == 2 )
			{
				// EnableISR();
				RVFinishRegop( state );
				return -21;
			}
		}
		*value = rval;

		if( ReadBitRVSWD( state ) != parity )
		{
			RVFinishRegop( state );
			return -23;
		}

		RVFinishRegop( state );
	}
#if defined(OLD_SWD_SUPPORT) && OLD_SWD_SUPPORT
	else if( state->opmode == 3 )
	{
		uint32_t reply = 0;
		SWDLongOP( state, command, &reply, 1 );
		*value = reply;
	}
#endif
	else
	{
		return -2;
	}
	return 0;
}

static inline void ConfigureIOForRVSWIO(struct SWIOState * state)
{
  return;
}

static inline void ConfigureIOForRVSWD(struct SWIOState * state)
{
  GPIO.out_w1ts = state->pinmaskC;
  GPIO.enable_w1ts = state->pinmaskC;
}

static void SetProtocolSpeed( struct SWIOState * state, enum ProtocolSpeed speed )
{
	switch( speed )
	{
		case PROTOCOL_SPEED_DEFAULT:
			state->t1coeff = SWD_T1;
			break;

		case PROTOCOL_SPEED_MEDIUM:
			state->t1coeff = SWD_T2;
			break;

		case PROTOCOL_SPEED_LOW:
			state->t1coeff = SWD_T3;
			break;
	}
	state->protocol_speed = speed;
}

#endif
