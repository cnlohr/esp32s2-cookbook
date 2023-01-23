#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "../hidapi.h"
#include "../hidapi.c"
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

// command-line arguments = file-list to watch.

struct timespec * file_timespecs;


int CheckTimespec( int argc, char ** argv )
{
	int i;
	int taint = 0;
	for( i = 1; i < argc; i++ )
	{
		struct stat sbuf;
		stat( argv[i], &sbuf);
#ifdef WIN32
		if( sbuf.st_mtime != file_timespecs[i].tv_sec )
		{
			file_timespecs[i].tv_sec = sbuf.st_mtime;
			taint = 1;
		}
#else
		if( sbuf.st_mtim.tv_sec != file_timespecs[i].tv_sec || sbuf.st_mtim.tv_nsec != file_timespecs[i].tv_nsec  )
		{
			file_timespecs[i] = sbuf.st_mtim;
			taint = 1;
		}
#endif
	}

	//Note: file_timespecs[0] unused at the moment.
	return taint;
}

int main( int argc, char ** argv )
{
	int first = 1;

	hid_init();
	hd = hid_open( VID, PID, L"usbsandbox000" );
	if( !hd ) { fprintf( stderr, "Could not open USB [interactive]\n" ); return -94; }
	file_timespecs = calloc( sizeof(struct timespec), argc );
	CheckTimespec( argc, argv );

#ifdef WIN32
	HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
	system(""); // enable VT100 Escape Sequence for WINDOWS 10 Ver. 1607 
#endif

	do
	{
		int r;

		// Disable tick.
		uint8_t rdata[513] = { 0 };
		rdata[0] = 172;
		r = hid_get_feature_report( hd, rdata, 513 );
#ifdef WIN32
		int toprint = r - 3;
#else
		int toprint = r - 2;
#endif

		if( r < 0 )
		{
			do
			{
				hd = hid_open( VID, PID, 0 );
				if( !hd )
					fprintf( stderr, "Could not open USB\n" );
				else
					fprintf( stderr, "Error: hid_get_feature_report failed with error %d\n", r );

				usleep( 100000 );
			} while( !hd );

			continue;
		}
		else if( toprint > 0 )
		{
#ifdef WIN32
			WriteConsoleA( hConsole, rdata+2, toprint, 0, 0 );
#else
			write( 1, rdata + 2, toprint );
#endif
		}

		// Check whatever else.
		int taint = CheckTimespec( argc, argv );
		if( ( taint || first ) && argc > 1 )
		{
			struct timespec spec_start, spec_end;

			uint8_t rdata[64];
			int r;
			int tries = 0;
			rdata[0] = 170;
			rdata[1] = 7;
			rdata[2] = 0 & 0xff;
			rdata[3] = 0 >> 8;
			rdata[4] = 0 >> 16;
			rdata[5] = 0 >> 24;
			do
			{
				r = hid_send_feature_report( hd, rdata, reg_packet_length );
				if( tries++ > 10 ) { fprintf( stderr, "Error sending feature report on command %d (%d)\n", rdata[1], r ); return -85; }
			} while ( r < 6 );
			tries = 0;

		    clock_gettime(CLOCK_REALTIME, &spec_start );
			system( "make run" );
		    clock_gettime(CLOCK_REALTIME, &spec_end );
			uint64_t ns_start = ((uint64_t)spec_start.tv_nsec) + ((uint64_t)spec_start.tv_sec)*1000000000LL;
			uint64_t ns_end = ((uint64_t)spec_end.tv_nsec) + ((uint64_t)spec_end.tv_sec)*1000000000LL;
			printf( "Elapsed: %.3f\n", (ns_end-ns_start)/1000000000.0 );
			first = 0;
		}
		
		usleep( 2000 );
	} while( 1 );

	hid_close( hd );
}

