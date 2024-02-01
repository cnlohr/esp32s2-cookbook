#ifndef _SIGGEN_H
#define _SIGGEN_H

#include "LoRa-SDR-Code.h"

static void SigSetupTest()
{
	
}

static uint32_t SigGen( uint32_t Frame240MHz, uint32_t codeTarg )
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

	// Determined experimentally, but this is the amount you have to divide the chip by to
	// Fully use the 125000 Hz channel Bandwidth.
	#define DESPREAD 100

	#define CHIPRATE 976 // SF7 (1.024 ms)
	#define CHIPSSPREAD (240000000/(CHIPRATE))

	// TODO: Get some of these encode things going: https://github.com/myriadrf/LoRa-SDR/blob/master/LoRaCodes.hpp

	// frame = 0...240000000

	// Let's say 1ms per sweep.
	int32_t sectionQuarterNumber = Frame240MHz / (CHIPSSPREAD/4);
	uint32_t placeInSweep = Frame240MHz % CHIPSSPREAD;
	// 2400 edge-to-edge = 
	if( sectionQuarterNumber < 0 ) return -codeTarg;

	sectionQuarterNumber -= 8*4;

	if( sectionQuarterNumber < 0 )
	{
		return ((placeInSweep /*+ 240000/2*/) % CHIPSSPREAD) / DESPREAD;			
	}


#if 1
	// https://static1.squarespace.com/static/54cecce7e4b054df1848b5f9/t/57489e6e07eaa0105215dc6c/1464376943218/Reversing-Lora-Knight.pdf
	// Says that this does not exist.  but, it does seem to exist in some of their waterfalls. 
	sectionQuarterNumber -= 4*2;
	// Two symbols
	if( sectionQuarterNumber < 0 )
	{
		int32_t  chirp = (8+sectionQuarterNumber)/4;
		int32_t  offset = chirp * 100000;
		return (( placeInSweep + offset) % 240000) / DESPREAD;			
	}
#endif

	sectionQuarterNumber -= 9;

	if( sectionQuarterNumber < 0 )
	{
		// Down-Sweeps for Sync
		return ((CHIPSSPREAD-placeInSweep/*+240000/2*/) % CHIPSSPREAD) / DESPREAD;				
	}

	if( sectionQuarterNumber < 200 )
	{
		uint32_t chirp = sectionQuarterNumber/4;
		uint32_t offset = chirp*200000;
		fplv = ((placeInSweep + offset) % CHIPSSPREAD) / DESPREAD;
		return fplv;
	}
	else
	{
		return -codeTarg;
	}
}

#endif

