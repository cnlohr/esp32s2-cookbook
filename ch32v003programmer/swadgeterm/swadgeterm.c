#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "../testapp/hidapi.h"
#include "../testapp/hidapi.c"
#include <sys/stat.h>

#include <time.h>

#define VID 0x303a
#define PID 0x4004

#ifdef WIN32
const int chunksize = 244;
const int force_packet_length = 255;
const int reg_packet_length = 65;
#else
const int chunksize = 244;
const int force_packet_length = 255;
const int reg_packet_length = 64;
#endif


hid_device * hd;

int main( int argc, char ** argv )
{
	hid_init();

#ifdef WIN32
	HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
	system(""); // enable VT100 Escape Sequence for WINDOWS 10 Ver. 1607 
#endif

	int toprint = 0;
	int r = -5;
	int already_waiting = 0;

	uint8_t rdata[513] = { 0 };
	do
	{
		if( r < 0 )
		{
			do
			{
				hd = hid_open( VID, PID, 0 );
				if( !hd )
				{
					if( !already_waiting )
					{
						fprintf( stderr, "Waiting for USB...\n" );
						already_waiting = 1;
					}
				}
				else
				{
					fprintf( stderr, "Connected.\n" );
				}
				usleep( 100000 );
			} while( !hd );
		}
		else if( toprint > 0 )
		{
			write( 1, rdata + 2, toprint );
		}
		// Disable tick.
		rdata[0] = 172;
		r = hid_get_feature_report( hd, rdata, force_packet_length );

		already_waiting = 0;

		if( r < 1 )
		{
			fprintf( stderr, "Error: hid_get_feature_report failed with error %d\n", r );
			continue;
		}

#ifdef WIN32
		toprint = r - 3;
#else
		toprint = r - 2;
#endif
	} while( 1 );

	hid_close( hd );
}

