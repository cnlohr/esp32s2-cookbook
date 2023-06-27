#include <stdint.h>
#include "hidapi.c"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


int main( int argc, char ** argv )
{
	int i;
	uint8_t rdata[300];
	
	if( argc < 2 )
	{
		fprintf( stderr, "Usage: testapp [picture file]\n" );
		return -9;
	}

	

	#define VID 0x303a
	#define PID 0x4004
	hid_init();
	hid_device * hd = hid_open( VID, PID, L"rbo128128g-610-wspi"); // third parameter is "serial"
	
	printf( "Hid device: %p\n", hd );
	if( !hd ) return -5;
	memset( rdata, 0, sizeof( rdata ) );
	uint8_t * rdataptr = rdata;

	*(rdataptr++) = 0xad; // Report ID.
	*(rdataptr++) = 0; //Will be elngth
	*(rdataptr++) = 0xff; // Report ID.


	*(rdataptr++) = 0x52; // LCD Bias
//	Write(COMMAND, 0x52); // Set LCD Bias=1/8 V0 (Experimentally found)

	*(rdataptr++) = 0x81; // Set Reference Voltage "Set Electronic Volume Register"
	*(rdataptr++) = 0x23; // Midway up.

	*(rdataptr++) = 0x00;
	*(rdataptr++) = 0x7B;
	*(rdataptr++) = 0x10; // Grayscale
	*(rdataptr++) = 0x00;
	*(rdataptr++) = 0xff; // teminate

	rdata[1] = (rdataptr-rdata)-2;

	int r = hid_send_feature_report( hd, rdata, 130 );
	printf( "%d\n", r );


	int w, h, n = 1;
	unsigned char *rgbbuffer = stbi_load(argv[1], &w, &h, 0, 1);

	int frame;
	int sec;
	if( !rgbbuffer )
	{
		fprintf( stderr, "Error: could not load %s\n", argv[1] );
		return -10;
	}
	if( w < 128 || h < 128 )
	{
		fprintf( stderr, "Error: Dimensions insufficient.\n" );
		return -9;
	}

	printf( "Dims: %d %d %d\n", w, h, n );
	for( sec = 0; sec < (128/8); sec++ )
	{
		int linesize = n * w;
		memset( rdata, 0, sizeof( rdata ) );
		rdataptr = rdata;
		*(rdataptr++) = 0xad; // Report ID.
		*(rdataptr++) = 255;
		*(rdataptr++) = sec;
		int i;
		for( i = 0; i < 256; i++ )
		{
			uint8_t * line = rgbbuffer + (sec * 8 * linesize ) + (i/2) * n;
			uint8_t ts = 0;
			int y;
			int rmask = 1<<(!(i&1));
			for( y = 0; y < 8; y++ )
			{
				int grey4 = (255-line[linesize * y]) / 64;
				ts |= ((grey4 & rmask)?1:0) << y;
			}
			*(rdataptr++) = ts;
		}
		int trysend = 258;
		int r = hid_send_feature_report( hd, rdata, trysend );
		if( r != trysend )
		{
			fprintf( stderr, "Error sending to device\n" );
			return -3;
		}
	}
	return 0;
}

