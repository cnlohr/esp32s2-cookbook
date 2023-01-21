#include <stdio.h>
#include <stdint.h>

#include "../hidapi.h"
#include "../hidapi.c"

#define VID 0x303a
#define PID 0x4004

hid_device * hd;

#ifdef WIN32
const int chunksize = 244;
const int force_packet_length = 255;
const int reg_packet_length = 65;
#else
const int chunksize = 244;
const int force_packet_length = 255;
const int reg_packet_length = 64;
#endif

const int alignlen = 4;
int tries = 0;

int main()
{
	int r;
	hid_init();
	hd = hid_open( VID, PID, 0);
	if( !hd ) { fprintf( stderr, "Could not open USB\n" ); return -94; }

	// Disable mode.
	uint8_t rdata[reg_packet_length];
	rdata[0] = 170;
	rdata[1] = 3; // AUSB_CMD_REBOOT
	rdata[2] = 1 & 0xff; // Yes, boot into bootloader.
	rdata[3] = 0 >> 8;
	rdata[4] = 0 >> 16;
	rdata[5] = 0 >> 24;
	do
	{
		r = hid_send_feature_report( hd, rdata, reg_packet_length );
		if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
	} while ( r < 6 );
	tries = 0;

	hid_close( hd );
#ifdef WIN32
	Sleep( 2000 );
#else
	usleep(200000);
#endif
	return 0;
}

