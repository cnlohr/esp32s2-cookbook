#ifndef _SIGGEN_H
#define _SIGGEN_H

#include <string.h>
#include "LoRa-SDR-Code.h"


// From SF6, i.e. 2 would be SF8
#define ADDSF 3

#define MAX_SYMBOLS 270

// https://electronics.stackexchange.com/questions/278192/understanding-the-relationship-between-lora-chips-chirps-symbols-and-bits
// Has some good hints

// At 125kHz
// https://medium.com/@prajzler/what-is-lora-the-fundamentals-79a5bb3e6dec
// With SF5, You get 3906 chips/second "Symbol Rate" (Included to help with math division)
// With SF7, You get 976 chips/second   / 5469 bits/sec

// 7 bits per symbol (I think)
// 7 * 4/5 = 5.6 data bits per symbol.
// https://wirelesspi.com/understanding-lora-phy-long-range-physical-layer/ says 7 for SF7

#define MARK_FROM_SF6 (1<<ADDSF)

// Determined experimentally, but this is the amount you have to divide the chip by to
// Fully use the 125000 Hz channel Bandwidth.
//#define DESPREAD (50*MARK_FROM_SF6)

#define CHIPRATE .000512 // SF7 (1.024 ms) / 976Chips/s
#define CHIPSSPREAD ((uint32_t)(240000000*MARK_FROM_SF6*CHIPRATE))

#define PREAMBLE_CHIRPS 10

int     symbols_len = 1;
uint16_t symbols[MAX_SYMBOLS];

uint32_t quadsetcount;
int32_t quadsets[MAX_SYMBOLS*4];

int32_t * AddChirp( int32_t * qso, int offset )
{
	offset = offset * CHIPSSPREAD / (MARK_FROM_SF6*64);
	*(qso++) = (CHIPSSPREAD * 0 / 4 + offset ) % CHIPSSPREAD;
	*(qso++) = (CHIPSSPREAD * 1 / 4 + offset ) % CHIPSSPREAD;
	*(qso++) = (CHIPSSPREAD * 2 / 4 + offset ) % CHIPSSPREAD;
	*(qso++) = (CHIPSSPREAD * 3 / 4 + offset ) % CHIPSSPREAD;
	return qso;
}

static void SigSetupTest()
{
	memset( symbols, 0, sizeof( symbols ) );
	symbols_len = 5;
	CreateMessageFromPayload( symbols, &symbols_len, MAX_SYMBOLS, 6+ADDSF );

	int j;
	//for( j = 0; j < symbols_len; j++ )
	//	symbols[j] = 255 - symbols[j];

	quadsetcount = 0;
	int32_t * qso = quadsets;
	for( j = 0; j < PREAMBLE_CHIRPS; j++ )
	{
		qso = AddChirp( qso, 0 );
	}

	uint8_t syncword = 0x43;

	qso = AddChirp( qso, ( syncword & 0xf ) << 3 );
	qso = AddChirp( qso, ( ( syncword & 0xf0 ) >> 4 ) << 3 );


	*(qso++) = -(CHIPSSPREAD * 0 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 1 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 2 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 3 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 0 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 1 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 2 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 3 / 4 )-1;
	*(qso++) = -(CHIPSSPREAD * 0 / 4 )-1;

	for( j = 0; j < symbols_len; j++ )
	{
		int ofs = symbols[j];
		//ofs = ofs ^ ((MARK_FROM_SF6<<6) -1);
		//ofs &= (MARK_FROM_SF6<<6) -1;
		qso = AddChirp( qso, ofs );
	}
	

	quadsetcount = qso - quadsets;
}

static int32_t SigGen( uint32_t Frame240MHz, uint32_t codeTarg )
{
	// TODO: Get some of these encode things going: https://github.com/myriadrf/LoRa-SDR/blob/master/LoRaCodes.hpp

	// frame = 0...240000000

	uint32_t sectionQuarterNumber = Frame240MHz / (CHIPSSPREAD/4);
	if( sectionQuarterNumber >= quadsetcount )
		return -codeTarg;

	int32_t quadValue = quadsets[sectionQuarterNumber];
	uint32_t placeInQuad = Frame240MHz % (CHIPSSPREAD/4);

	if( quadValue >= 0 )
		return ( ( quadValue + placeInQuad ) % CHIPSSPREAD); // Up-Chirp
	else
		return ( ( quadValue - placeInQuad + CHIPSSPREAD ) % CHIPSSPREAD ); // Down-Chirp

#if 0
	// Let's say 1ms per sweep.
	int32_t sectionQuarterNumber = Frame240MHz / (CHIPSSPREAD/4);
	uint32_t placeInSweep = Frame240MHz % CHIPSSPREAD;
	// 2400 edge-to-edge = 
	if( sectionQuarterNumber < 0 ) return -codeTarg;

	sectionQuarterNumber -= PREAMBLE_CHIRPS*4;

	// Preamble Start
	if( sectionQuarterNumber < 0 )
	{
		return ((placeInSweep /*+ 240000/2*/) % CHIPSSPREAD) / DESPREAD;			
	}

	// Last 2 codes here are for the sync word.

#define SYNC_WORD
#ifdef SYNC_WORD
	// https://static1.squarespace.com/static/54cecce7e4b054df1848b5f9/t/57489e6e07eaa0105215dc6c/1464376943218/Reversing-Lora-Knight.pdf
	// Says that this does not exist.  but, it does seem to exist in some of their waterfalls. 
	sectionQuarterNumber -= 4*2;
	// Two symbols
	if( sectionQuarterNumber < 0 )
	{
		int32_t  chirp = (8+sectionQuarterNumber)/4;
		uint32_t SYNCWORD = (((0x34)>>(4-chirp*4)) & 0xf)<<3; //0x34 for some vendors?
		int32_t  offset = SYNCWORD * CHIPSSPREAD / (MARK_FROM_SF7*128);
		return (( placeInSweep + offset) % CHIPSSPREAD) / DESPREAD;			
	}
#endif
/*

	EXTRA NOTES: GNURadio says to look for sync word 18
*/



	sectionQuarterNumber -= 9;

	if( sectionQuarterNumber < 0 )
	{
		// Down-Sweeps for Sync
		return ((CHIPSSPREAD-placeInSweep/*+240000/2*/) % CHIPSSPREAD) / DESPREAD;				
	}

	uint32_t chirp = (sectionQuarterNumber)/4;
	if( chirp < symbols_len )
	{
		placeInSweep += (CHIPSSPREAD*3/4);
		uint32_t offset = ( symbols[chirp] + 0 ) * CHIPSSPREAD / (MARK_FROM_SF7*128);
		fplv = ((placeInSweep + offset) % CHIPSSPREAD) / DESPREAD;
		return fplv;
	}
	else
	{
		return -codeTarg;
		//return -1;
	}
#endif
}

/*
  A real semtech SX1276 will produce 16 symbols at SF8 if
  Given 2 byte payload, CRC explicit header.
  Given 3 bytes of payload, that goes up to 24 symbols.

	[HEADER] [HEADER*0.5] [HEADER] **An extra byte** [PAYLOAD] [PAYLOAD] [PAYLOAD] [CRC] [CRC] << Does not fit.

	Payload sizes:
		-5 would be 8       (Considering CRC would be -3 bytes)
		-4 would be 8
		-3 would be 8
		-2 would be 8
		-1 would be 16
		0 would be 16
		1 byte: 16 symbols. (Include CRC so 3 bytes)
		2 byte: 16 symbols. (Include CRC so 4 bytes)
			// theoreteically, 2 bytes  + 2 crc should fit nicely into 16 symbols (or 8 bytes) BUT 3+2 (5) does not.

		3 byte: 24 symbols. (Include CRC so 5 bytes)
		4 byte: 24 symbols
		5 byte: 24 symbols.
		6 byte: 24 symbols.
		7 byte: 32 symbols.
		8 byte: 32 symbols.
		9 byte: 32 symbols.
		10 byte: 32 symbols.
		11 byte: 40 symbols.

	That means there's 

*/

#endif

