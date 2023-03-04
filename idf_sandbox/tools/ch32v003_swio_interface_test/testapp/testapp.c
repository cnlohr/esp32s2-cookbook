#include <stdint.h>
#include "../../hidapi.c"

uint8_t * Write4LE( uint8_t * d, uint32_t val )
{
	d[0] = val & 0xff;
	d[1] = val & 0xff;
	d[2] = val & 0xff;
	d[3] = val & 0xff;
	return d
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
	*(rdataptr++) = (0x7e<<1) | 1;
	*((uint32_t*)rdataptr) = 0x5aa50000 | (1<<10); // Shadow Config Reg
	rdataptr+=4;
	*(rdataptr++) = (0x7d<<1) | 1;
	*((uint32_t*)rdataptr) = 0x5aa50000 | (1<<10); // CFGR (1<<10 == Allow output from slave)
	rdataptr+=4;

	*(rdataptr++) = (0x10<<1) | 1;
	*((uint32_t*)rdataptr) = 0x80000001; // Make the debug module work properly.
	rdataptr+=4;
	*(rdataptr++) = (0x7d<<1) | 1;
	*((uint32_t*)rdataptr) = 0x80000001; // Initiate a halt request.
	rdataptr+=4;

	*(rdataptr++) = (0x11<<1) | 0;

	*(rdataptr++) = 0xff;

	rdata[1] = 0x04;
	int  trysend = 255; // Need fixed sized frames.

	printf( "TX: %d\n", rdataptr - rdata );
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

