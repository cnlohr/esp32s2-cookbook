#include <stdint.h>
#include "../../hidapi.c"

int main()
{
	int i;
	uint8_t rdata[300];
	
	#define VID 0x303a
	#define PID 0x4004
	hid_init();
	hid_device * hd = hid_open( VID, PID, L"usbsandbox000"); // third parameter is "serial"
	
	printf( "Hid device: %p\n", hd );
	if( !hd ) return -5;

	memset( rdata, 0, sizeof( rdata ) );
	uint8_t * rdataptr = rdata;
	*(rdataptr++) = 0xad; // Report ID.
	*(rdataptr++) = 0xff; // Report ID.


	*(rdataptr++) = 0x53; // LCD Bias
//	Write(COMMAND, 0x52); // Set LCD Bias=1/8 V0 (Experimentally found)

	*(rdataptr++) = 0x81; // Set Reference Voltage "Set Electronic Volume Register"
	*(rdataptr++) = 0x36; // Midway up.
	*(rdataptr++) = 0xff; // teminate

	int r = hid_send_feature_report( hd, rdata, 130 );
	printf( "%d\n", r );


	int frame;
	int sec;
	for( frame = 0; frame < 60; frame++ )
	{
		for( sec = 0; sec < (128/8); sec++ )
		{
			memset( rdata, 0, sizeof( rdata ) );
			rdataptr = rdata;
			*(rdataptr++) = 0xad; // Report ID.
			*(rdataptr++) = sec;
			int i;
			for( i = 0; i < 128; i++ )
				*(rdataptr++) = rand();
			int trysend = 130;
			int r = hid_send_feature_report( hd, rdata, trysend );
			printf( "%d\n", r );
		}
	}
}

