#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../hidapi.h"
#include "../hidapi.c"

#define VID 0x303a
#define PID 0x4004

hid_device * hd;

int tries = 0;
int alignlen = 4;

int req_sandbox = 1024;

uint8_t rdata[2048]; //Must be plenty big.

int main( int argc, char ** argv )
{
	int r;
	hid_init();
	hd = hid_open( VID, PID, 0);
	if( !hd ) { fprintf( stderr, "Could not open USB\n" ); return -94; }


	// Disable mode.
	uint8_t rdata[6];
	rdata[0] = 170;
	rdata[1] = 7;
	rdata[2] = 0 & 0xff;
	rdata[3] = 0 >> 8;
	rdata[4] = 0 >> 16;
	rdata[5] = 0 >> 24;
	do
	{
		r = hid_send_feature_report( hd, rdata, 65 );
		if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r < 6 );
	tries = 0;

	// Give it a chance to exit.
	usleep( 20000 );
		
	// First, get the current size/etc. of the memory allocated for us.
	rdata[0] = 170;
	rdata[1] = 8;
	rdata[2] = req_sandbox&0xff;
	rdata[3] = (req_sandbox>>8)&0xff;
	rdata[4] = (req_sandbox>>16)&0xff;
	rdata[5] = (req_sandbox>>24)&0xff;
	do
	{
		r = hid_send_feature_report( hd, rdata, 65 );
		if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r != 65 );
	tries = 0;
	do
	{
		rdata[0] = 170;
		r = hid_get_feature_report( hd, rdata, 512 );
		if( tries++ > 10 ) { fprintf( stderr, "Error reading feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r < 10 );
	uint32_t allocated_addy = ((uint32_t*)(rdata+0))[0];
	uint32_t allocated_size = ((uint32_t*)(rdata+0))[1];

	printf( "Allocation: %08x %d\n", allocated_addy, allocated_size );
	
	/*
	    AUSB_CMD_WRITE_RAM: 
        Parameter 0: Address to start writing to.
		Parameter 1 [2-bytes]: Length of data to write
        Data to write...
    
		AUSB_CMD_READ_RAM: 0x05
        Parameter 0: Address to start reading from.
        (Must use "get report" to read data)
	*/
	int wl = 128;
	printf( "to sendpoint 172\n" );

	// Write
	rdata[0] = 172;
	rdata[1] = 0x04;
	rdata[2] = allocated_addy & 0xff;
	rdata[3] = allocated_addy >> 8;
	rdata[4] = allocated_addy >> 16;
	rdata[5] = allocated_addy >> 24;
	rdata[6] = wl & 0xff;
	rdata[7] = wl >> 8;

	int  trysend = 48;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 65;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 129;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 256;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );

	printf( "and to sendpoint 171\n" );
	// Write
	rdata[0] = 171;
	rdata[1] = 0x04;
	rdata[2] = allocated_addy & 0xff;
	rdata[3] = allocated_addy >> 8;
	rdata[4] = allocated_addy >> 16;
	rdata[5] = allocated_addy >> 24;
	rdata[6] = wl & 0xff;
	rdata[7] = wl >> 8;

	trysend = 48;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 65;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 129;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 254;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 255;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 256;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 257;
	rdata[0] = 171;
	usleep( 1000 );
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	
	
	


	printf( "and to sendpoint 170\n" );
	// Write
	rdata[0] = 170;
	rdata[1] = 0x04;
	rdata[2] = allocated_addy & 0xff;
	rdata[3] = allocated_addy >> 8;
	rdata[4] = allocated_addy >> 16;
	rdata[5] = allocated_addy >> 24;
	rdata[6] = wl & 0xff;
	rdata[7] = wl >> 8;

	trysend = 48;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 65;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 129;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );
	trysend = 256;
	r = hid_send_feature_report( hd, rdata, trysend );
	printf( "Trysend: %d -> %d\n", trysend, r );

	
	printf( "Get Tests\n" );
	rdata[0] = 172;
	trysend = 512;
	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "Tryget fr %d: %d -> %d\n", rdata[0], trysend, r );
	rdata[0] = 171;
	trysend = 512;
	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "Tryget fr %d: %d -> %d\n", rdata[0], trysend, r );
	rdata[0] = 170;
	trysend = 512;
	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "Tryget fr %d: %d -> %d\n", rdata[0], trysend, r );


	printf( "Get Tests\n" );
	rdata[0] = 172;
	trysend = 10;
	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "Tryget fr %d: %d -> %d\n", rdata[0], trysend, r );
	rdata[0] = 171;
	trysend = 10;
	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "Tryget fr %d: %d -> %d\n", rdata[0], trysend, r );
	rdata[0] = 170;
	trysend = 10;
	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "Tryget fr %d: %d -> %d\n", rdata[0], trysend, r );
	
	
}

