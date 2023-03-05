#include <stdint.h>
#include "../../hidapi.c"

uint8_t * Write4LE( uint8_t * d, uint32_t val )
{
	d[0] = val & 0xff;
	d[1] = (val>>8) & 0xff;
	d[2] = (val>>16) & 0xff;
	d[3] = (val>>24) & 0xff;
	return d+4;
}

uint8_t * Write2LE( uint8_t * d, uint32_t val )
{
	d[0] = val & 0xff;
	d[1] = (val>>8) & 0xff;
	return d+2;
}

int main()
{
	int i;
	uint8_t rdata[300];
	
	#define VID 0x303a
	#define PID 0x4004
	hid_init();
	hid_device * hd = hid_open( VID, PID, L"usbsandbox000"); // third parameter is "serial"
	
	printf( "Hid device: %p\n", hd );

	memset( rdata, 0, sizeof( rdata ) );
	uint8_t * rdataptr = rdata;
	*(rdataptr++) = 0xad; // Report ID.



	*(rdataptr++) = 0xfe;
	*(rdataptr++) = 0x03; // Power Up
	*(rdataptr++) = 0xfe;
	*(rdataptr++) = 0x04; // Delay-for-us
	rdataptr = Write2LE( rdataptr, 16000 ); // Neds to wait at least 16ms from powerdown.

//	*(rdataptr++) = 0xfe;
//	*(rdataptr++) = 0x01; // Do song and dance.  I don't know the purpose.

	*(rdataptr++) = (0x7e<<1) | 1;
	rdataptr = Write4LE( rdataptr, 0x5aa50000 | (1<<10) ); // Shadow Config Reg
	*(rdataptr++) = (0x7d<<1) | 1;
	rdataptr = Write4LE( rdataptr, 0x5aa50000 | (1<<10) ); // CFGR (1<<10 == Allow output from slave)
	*(rdataptr++) = (0x7d<<1) | 1;
	rdataptr = Write4LE( rdataptr, 0x5aa50000 | (1<<10) ); // Bug in silicon?  If coming out of cold boot, and we don't do our little "song and dance" this has to be called.

	*(rdataptr++) = (0x10<<1) | 1;
	rdataptr = Write4LE( rdataptr, 0x80000001 ); // Make the debug module work properly.
	*(rdataptr++) = (0x7d<<1) | 1;
	rdataptr = Write4LE( rdataptr, 0x80000001 ); // Initiate a halt request.

	// Read from 0x11
	*(rdataptr++) = (0x11<<1) | 0;

	*(rdataptr++) = 0xfe;
	*(rdataptr++) = 0x02; // Power Down

	// Terminate.
	*(rdataptr++) = 0xff;

	int  trysend = 255; // Need fixed sized frames.

	printf( "TX: %d\n", (int)(rdataptr - rdata) );
	for( i = 0; i < 256; i++ )
	{
		printf( "%02x %c", rdata[i], ((i&0xf)==0xf)?'\n':' ');
	}

	int r = hid_send_feature_report( hd, rdata, trysend );
	printf( "%d\n", r );

	r = hid_get_feature_report( hd, rdata, trysend );
	printf( "got: %d\n", r );
	
	for( i = 0; i < r; i++ )
	{
		printf( "%02x %c", rdata[i], ((i&0xf)==0xf)?'\n':' ');
	}
	printf( "\n" );

}

