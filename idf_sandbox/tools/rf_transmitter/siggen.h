#ifndef _SIGGEN_H
#define _SIGGEN_H

#include <string.h>
#include "LoRa-SDR-Code.h"


#define ADDSF 1 // SF8 = 1

#define MAX_SYMBOLS 200
int     symbols_len = 1;
uint16_t symbols[MAX_SYMBOLS];

static void SigSetupTest()
{
	memset( symbols, 0, sizeof( symbols ) );
	symbols_len = 5;
	CreateMessageFromPayload( symbols, &symbols_len, MAX_SYMBOLS, 7+ADDSF );

	int j;
	//for( j = 0; j < symbols_len; j++ )
	//	symbols[j] = 255 - symbols[j];

//	int j;
	for( j = 0; j < symbols_len; j++ )
	{
		//symbols[j] = 255 - symbols[j] ;
		//symbols[j] =  255 - (uint8_t)(symbols[j] + 0);
		uprintf( "%d: %02x\n", j, symbols[j] );
	}
}

static int32_t SigGen( uint32_t Frame240MHz, uint32_t codeTarg )
{
	uint32_t fplv = 0;
	// https://electronics.stackexchange.com/questions/278192/understanding-the-relationship-between-lora-chips-chirps-symbols-and-bits
	// Has some good hints

	// At 125kHz
	// https://medium.com/@prajzler/what-is-lora-the-fundamentals-79a5bb3e6dec
	// With SF5, You get 3906 chips/second "Symbol Rate" (Included to help with math division)
	// With SF7, You get 976 chips/second   / 5469 bits/sec

	// 7 bits per symbol (I think)
	// 7 * 4/5 = 5.6 data bits per symbol.
	// https://wirelesspi.com/understanding-lora-phy-long-range-physical-layer/ says 7 for SF7

	// If SF8 set to 2, if SF9 4, if SF6, set to 0.5
	#define MARK_FROM_SF7 (1<<ADDSF)

	// Determined experimentally, but this is the amount you have to divide the chip by to
	// Fully use the 125000 Hz channel Bandwidth.
	#define DESPREAD (100*MARK_FROM_SF7)

	#define CHIPRATE .001024 // SF7 (1.024 ms) / 976Chips/s
	#define CHIPSSPREAD ((uint32_t)(240000000*MARK_FROM_SF7*CHIPRATE))

	// TODO: Get some of these encode things going: https://github.com/myriadrf/LoRa-SDR/blob/master/LoRaCodes.hpp

	// frame = 0...240000000

	// Let's say 1ms per sweep.
	int32_t sectionQuarterNumber = Frame240MHz / (CHIPSSPREAD/4);
	uint32_t placeInSweep = Frame240MHz % CHIPSSPREAD;
	// 2400 edge-to-edge = 
	if( sectionQuarterNumber < 0 ) return -codeTarg;

	sectionQuarterNumber -= 8*4;

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
		uint32_t SYNCWORD = 0; //0x34 for some vendors?
		int32_t  chirp = (8+sectionQuarterNumber)/4;
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
		//return -codeTarg;
		return -1;
	}
}

#endif
