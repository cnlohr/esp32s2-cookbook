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

static int UPDIComputeClocksPerBit( int cpumhz, int baudrate );
static int UPDISetup( uint32_t pinmask, int cpumhz, int clocks_per_bit, uint8_t sib[16] );
static inline void UPDIPowerOff( int pinmask, int pinmaskpower );
static inline void UPDIPowerOn( int pinmask, int pinmaskpower );
static int UPDIErase( uint32_t pinmask, int clocks_per_bit );
static int UPDIReset( uint32_t pinmask, int clocks_per_bit );
static int UPDIFlash( uint32_t pinmask, int clocks_per_bit, int memory_address, uint8_t * program, int len, int flash_bootloader );
static int UPDIReadMemoryArea( uint32_t pinmask, int clocks_per_bit, int memory_location, uint8_t * program, int len );



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
	GPIO.out_w1ts = pinmask;
	GPIO.enable_w1ts = pinmask;
	DelayUntil( Ctarget );
	Ctarget += counts_per_bit;
	for( i = 0; i < 12; i++ )
	{

		volatile uint32_t * gp = (word&1) ? &GPIO.out_w1ts : &GPIO.out_w1tc;
		*gp = pinmask;
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
	esp_rom_delay_us(200);
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

#define UPDI_MEM_INCREMENT 0x4

static int UPDICheckAddress( uint32_t pinmask, int clocks_per_bit, int * updiptr, int flags, int address )
{
	int r = 0x40;
	if( !updiptr || address != *updiptr )
	{
		UPDISendWord( pinmask, clocks_per_bit, 0x55 );
		UPDISendWord( pinmask, clocks_per_bit, 0x69 );
		UPDISendWord( pinmask, clocks_per_bit, (address)&0xff );
		UPDISendWord( pinmask, clocks_per_bit, (address>>8) );
		r = UPDIReceive( pinmask, clocks_per_bit );
		esp_rom_delay_us(100);
	}

	if( updiptr )
	{
		*updiptr = address  + !!( flags & UPDI_MEM_INCREMENT );
	}

	return r != 0x40;
}

static int UPDIReadMemory( uint32_t pinmask, int clocks_per_bit, int * updiptr, int flags, int address  )
{
	UPDICheckAddress( pinmask, clocks_per_bit, updiptr, flags, address );

	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0x20 + flags );
	int r = UPDIReceive( pinmask, clocks_per_bit );
	esp_rom_delay_us(100);
	return r;
}

static int UPDIWriteMemory( uint32_t pinmask, int clocks_per_bit, int * updiptr, int flags, int address, int data )
{
	UPDICheckAddress( pinmask, clocks_per_bit, updiptr, flags, address );

	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0x60 + flags );
	UPDISendWord( pinmask, clocks_per_bit, data & 0xff );
	int r = UPDIReceive( pinmask, clocks_per_bit );
	esp_rom_delay_us(100);
	return r != 0x40;
}

static int UPDIComputeClocksPerBit( int cpumhz, int baudrate )
{
	return ( cpumhz * 1000000 + (baudrate/2) ) / baudrate;
}

static int UPDISetup( uint32_t pinmask, int cpumhz, int clocks_per_bit, uint8_t sib[16] )
{
	int r;
	int tries = 0;

retry:
	r = UPDISyncStart( pinmask, cpumhz );
	if( r )
		return r;

	UPDISendBreak( pinmask, cpumhz );


	// Receive SIB

	r = UPDITransact( pinmask, clocks_per_bit, 0xe5, 16, sib );
	if( r != 16 )
	{
		if( tries++ < 10 ) goto retry;
		return r;
	}

	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc2\x05", 3 ); // STCS, UPDI.CTRLA = 0x6 (very low guard time)

retry2:

	// Unlock flash

	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0xe0 );
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)" gorPMVN", 8 ); //NVMErase", backwards.


	uint8_t csret = 0;
	r = UPDITransact( pinmask, clocks_per_bit, 0x87, 1, &csret );  //LDCS, UPDI.ASI_KEY_STATUS -> Look for NVMPROG

	if( r != 1 )
		return -12;

	if( ! (csret & 0x10) ) // NVMPROG
	{
		if( tries++ < 10 )
		{
			UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x59", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x59
			UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x00", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x00 Clear reset request.
			goto retry2;
		}
		return -13;
	}


retry3: 
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
	{
		if( tries++ < 10 ) goto retry3;
		return -15;
	}

#if 0
	int to_max = 10;
	int to;
	int memory_offset = 0x8000;
	int updi_ptr = -1;

	// First, check BOOTEND.
	if( !flash_bootloader )
	{
		r = UPDIReadMemory( pinmask, clocks_per_bit, &updi_ptr, 0, 0x1280 /*FUSE*/ + 8 /* BOOTEND */ );
		if( r < 0 )
			return r;
		memory_offset += r<<8;
	}
#endif

	return 0;
}

static int UPDIErase( uint32_t pinmask, int clocks_per_bit )
{
	int tries = 0;

retrya:
	UPDISendWord( pinmask, clocks_per_bit, 0x55 );
	UPDISendWord( pinmask, clocks_per_bit, 0xe0 );
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"esarEMVN", 8 ); //NVMErase", backwards.

	uint8_t csret = 0;
	int r = UPDITransact( pinmask, clocks_per_bit, 0x87, 1, &csret );  //LDCS, UPDI.ASI_KEY_STATUS -> Look for CHIPERASE

	if( r != 1 )
	{
		if( tries++ < 10 ) goto retrya;
		return -12;
	}

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

static int UPDIFlash( uint32_t pinmask, int clocks_per_bit, int memory_address, uint8_t * program, int len, int flash_bootloader )
{
	int r;

	const int blocksize = 64;


	int updi_ptr = -1;
	int to_max = 10;
	int to;
	int offset = 0;
	int block = 0;
	for( block = 0; block < ( len + blocksize - 1 ) / blocksize; block++ )
	{
		r = UPDIWriteMemory( pinmask, clocks_per_bit, &updi_ptr, UPDI_MEM_INCREMENT, 0x1000 + 8 /*NVMCTRL.ADDR*/, memory_address & 0xff );
		r = UPDIWriteMemory( pinmask, clocks_per_bit, &updi_ptr, UPDI_MEM_INCREMENT, 0x1000 + 9 /*NVMCTRL.ADDR*/, memory_address >> 8 );

		// Page size is 64 bytes.
		r = UPDIWriteMemory( pinmask, clocks_per_bit, &updi_ptr, UPDI_MEM_INCREMENT, 0x1000 + 0 /*NVMCTRL.CTRLA*/, 0x04 ); // Page Buffer Clear

		int bo;
		for( bo = 0; bo < blocksize; bo++ )
			r = UPDIWriteMemory( pinmask, clocks_per_bit, &updi_ptr, UPDI_MEM_INCREMENT, memory_address + bo, ((bo+offset)<len)?program[bo + offset]:0 );

		r = UPDIWriteMemory( pinmask, clocks_per_bit, &updi_ptr, UPDI_MEM_INCREMENT, 0x1000 + 0 /*NVMCTRL.CTRLA*/, 0x03 ); // erase + write page

		for( to = 0; to < to_max; to++ )
		{
			r = UPDIReadMemory( pinmask, clocks_per_bit, &updi_ptr, 0, 0x1000 + 2 );
			if( r & 0x40 ) continue; // Invalid Read
			if( r & 0x01 ) continue; // FBUSY
			break;
		}

		r = UPDIReadMemory( pinmask, clocks_per_bit, &updi_ptr, 0, 0x1000 + 2 );
		if( r != 0x00 || to == to_max )
			return -19;

		offset += blocksize;
		memory_address += blocksize;
	}

	return 0;
}

static int UPDIReadMemoryArea( uint32_t pinmask, int clocks_per_bit, int memory_location, uint8_t * program, int len )
{
	int i, r;
	int updi_ptr = -1;
	for( i = 0; i < len; i++ )
	{
		r = UPDIReadMemory( pinmask, clocks_per_bit, &updi_ptr, UPDI_MEM_INCREMENT, memory_location + i );
		if( r < 0 ) return r;
		program[i] = r;
	}
	return 0;
}

static int UPDIReset( uint32_t pinmask, int clocks_per_bit )
{
	// Reset chip.
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x59", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x59
	UPDISendString( pinmask, clocks_per_bit, (const uint8_t*)"\x55\xc8\x00", 3 ); // STCS, UPDI.ASI_RESET_REQ = 0x00 Clear reset request.

	int retry;
	const int max_retry = 100;
	uint8_t csret = 0;
	int r = 0;

	for( retry = 0; retry < max_retry; retry++ )
	{
		r = UPDITransact( pinmask, clocks_per_bit, 0x8B, 1, &csret );  //LDCS, UPDI.ASI_SYS_STATUS -> Look for NVMPROG
		if( r != 1 )
			return r;
		if( !(csret & 0x20) ) //RSTSYS
			break;
	}
	return 0;
}


#endif

