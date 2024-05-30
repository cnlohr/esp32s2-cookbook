#include <stdint.h>
#include "../testapp/hidapi.c"



uint8_t * Write2LE( uint8_t * d, uint32_t val )
{
	d[0] = val & 0xff;
	d[1] = (val>>8) & 0xff;
	return d+2;
}

int64_t SimpleReadNumberInt( const char * number, int64_t defaultNumber )
{
	if( !number || !number[0] ) return defaultNumber;
	int radix = 10;
	if( number[0] == '0' )
	{
		char nc = number[1];
		number+=2;
		if( nc == 0 ) return 0;
		else if( nc == 'x' ) radix = 16;
		else if( nc == 'b' ) radix = 2;
		else { number--; radix = 8; }
	}
	char * endptr;
	uint64_t ret = strtoll( number, &endptr, radix );
	if( endptr == number )
	{
		return defaultNumber;
	}
	else
	{
		return ret;
	}
}

int main( int argc, char ** argv )
{
	char ** argvptr = argv;
	uint8_t rdata[300];
	int i;
	int r;
	#define VID 0x303a
	#define PID 0x4004
	hid_init();
	hid_device * hd = hid_open( VID, PID, L"s2-ch32xx-pgm-v0"); // third parameter is "serial"
	if( !hd )
	{
		fprintf( stderr, "Error: Could not open device\n" );
		return -11;
	}

	if( argc < 2 ) goto errorwarning;

	argvptr++;
	uint8_t * rdataptr;
	int  trysend;


reinit:
	memset( rdata, 0xff, sizeof( rdata ) );
	rdataptr = rdata;
	*(rdataptr++) = 0xad; // Report ID.
	*(rdataptr++) = 0xfd;
	*(rdataptr++) = 1; // UPDI mode
	r = hid_send_feature_report( hd, rdata, trysend );
	if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
	r = hid_get_feature_report( hd, rdata, trysend );
	if( r < 0 ) { fprintf( stderr, "Error getting request\n" ); return -1; };
/*
	for( i = 0; i < r; i++ )
	{
		printf( "%02x %c", rdata[i], ((i&0xf)==0xf)?'\n':' ');
	}
	printf( "\n" );
*/



	memset( rdata, 0xff, sizeof( rdata ) );
	rdataptr = rdata;
	*(rdataptr++) = 0xad; // Report ID.
	*(rdataptr++) = 0x90;
	rdataptr = Write2LE( rdataptr, 115200/32 );

	trysend = 255; // Need fixed sized frames.

/*
	printf( "TX: %d\n", (int)(rdataptr - rdata) );
	for( i = 0; i < 256; i++ )
	{
		printf( "%02x %c", rdata[i], ((i&0xf)==0xf)?'\n':' ');
	}
*/

	r = hid_send_feature_report( hd, rdata, trysend );
	if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };

	r = hid_get_feature_report( hd, rdata, trysend );
	if( r < 0 ) { fprintf( stderr, "Error getting request\n" ); return -1; };

/*	for( i = 0; i < r; i++ )
	{
		printf( "%02x %c", rdata[i], ((i&0xf)==0xf)?'\n':' ');
	}
	printf( "\n" );
*/

	if( rdata[0] != 18 && rdata[1] == 0 ) 
	{
		fprintf( stderr, "Error: weird response from setup; got %d bytes, %d code\n", rdata[0], rdata[1] );
		return -13;
	}
	rdata[18] = 0;
	printf( "UPDI Code: %s\n", rdata+2 );

	while( argvptr - argv < argc )
	{
		int operation = -1;
		if( strcmp( argvptr[0], "READ" ) == 0 ) operation = 0;
		else if( strcmp( argvptr[0], "WRITE" ) == 0 ) operation = 1;
		else if( strcmp( argvptr[0], "ERASE" ) == 0 ) operation = 2;
		else if( strcmp( argvptr[0], "BOOT" ) == 0 ) operation = 3;

		if( operation < 0 )
		{
			fprintf( stderr, "Unknown command %s\n", argvptr[0] );
			goto errorwarning;
		}

		argvptr++;

		switch( operation )
		{
		case 0:
			if( argvptr - argv + 3 > argc )
			{
				fprintf( stderr, "Error: Not enough parameters\n" );
				goto errorwarning;
			}

			int64_t addy = SimpleReadNumberInt( *(argvptr++), -1 );
			if( addy < 0 || addy > 0xffff )
			{
				fprintf( stderr, "Error: Address invalid for READ statement\n" );
				goto errorwarning;				
			}

			int64_t len = SimpleReadNumberInt( *(argvptr++), -1 );
			if( len < 0 || len > 0xffff )
			{
				fprintf( stderr, "Error: Size invalid for READ statement\n" );
				goto errorwarning;				
			}

			char * fname = *(argvptr++);
			int fmode = 0;
			FILE * f = 0;
			if( strcmp( fname, "-" ) == 0 )	fmode = 1;
			else if( strcmp( fname, "+" ) == 0 ) fmode = 2;
			else
			{
				f = fopen( fname, "wb" );
				if( !f || ferror( f ) )
				{
					fprintf( stderr, "Error: failed to open %s\n", fname );
					return -53;
				}
			}

			int blocks = (len + 63)/64;
			int b;
			for( b = 0; b < blocks;b++ )
			{
				int left = len - b*64;
				if( left > 64 ) left = 64;
				memset( rdata, 0xff, sizeof( rdata ) );
				rdataptr = rdata;
				*(rdataptr++) = 0xad; // Report ID.
				*(rdataptr++) = 0x94;
				*(rdataptr++) = addy & 0xff;
				*(rdataptr++) = addy >> 8;
				*(rdataptr++) = left;

				rdataptr = Write2LE( rdataptr, 16000 );
				trysend = 255; // Need fixed sized frames.
				r = hid_send_feature_report( hd, rdata, trysend );
				if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
				r = hid_get_feature_report( hd, rdata, trysend );
				if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };

/*
	printf( "got: %d\n", r );
	for( i = 0; i < r; i++ )
	{
		printf( "%02x %c", rdata[i], ((i&0xf)==0xf)?'\n':' ');
	}
	printf( "\n" );
*/
				if( rdata[0] == left + 2 || rdata[1] != 0 )
				{
					fprintf( stderr, "Error reading block %d (%04x)\n", b, addy );
					return -9;
				}

				switch( fmode )
				{
				case 0: 
					printf( "." );
					fwrite( rdata+2, 1, left, f );
					break;
				case 1:
					fwrite( rdata+2, 1, left, stdout );
					break;
				case 2:
					for( i = 0; i < left; i+=16 )
					{
						int j;
						printf( "%04x:", (int)(addy + i*16) );
						int lefthere = left - i;
						for( j = 0; j < 16; j++ )
						{
							if( j == 8 ) printf( " " );
							if( j < lefthere )
								printf( " %02x", rdata[2+i+j] );
							else
								printf( "   " );
						}
						printf( " | " );
						for( j = 0; j < 16; j++ )
						{
							if( j < lefthere )
							{
								int c = rdata[2+i+j];
								if( c < 32 || c > 126 ) c = '.';
								printf( "%c", c );
							}
							else
							{
								printf( " ");
							}
						}
						printf( "\n" );
					}
					
				}
				fflush( stdout );
				addy += 64;
			}
			if( f )
				fclose( f );

			printf( "\nWrote %d blocks\n", blocks );
			break;
		case 1:
		{
			if( argvptr - argv + 2 > argc )
			{
				fprintf( stderr, "Error: Not enough parameters\n" );
				goto errorwarning;
			}

			int64_t addy = SimpleReadNumberInt( *(argvptr++), -1 );
			if( addy < 0 || addy > 0xffff )
			{
				fprintf( stderr, "Error: Could not read valid WRITE statement\n" );
				goto errorwarning;				
			}
			if( addy & 0x3f )
			{
				fprintf( stderr, "Error: Addy is not divisible by 64\n" );
				return -44;
			}

			const char * fname = *(argvptr++);
			FILE * f = fopen( fname, "rb" );
			if( !f || ferror(f ) )
			{
				fprintf( stderr, "Error opening %s\n", fname );
				return -45;
			}
			fseek( f, 0, SEEK_END );
			int len = ftell( f );
			fseek( f, 0, SEEK_SET );

			int blocks = (len + 63)/64;
			int b;
			for( b = 0; b < blocks;b++ )
			{
				int left = len - b * 64;
				if( left > 64 ) left = 64;

				memset( rdata, 0xff, sizeof( rdata ) );
				rdataptr = rdata;
				*(rdataptr++) = 0xad; // Report ID.
				*(rdataptr++) = 0x93;
				*(rdataptr++) = addy & 0xff;
				*(rdataptr++) = addy >> 8;
				int rr = fread( rdataptr, 1, left, f );
				if( rr != left ) {
					fprintf( stderr, "Error reading input file (%d/%d) (block: %d)\n", rr, left, b );
					return -44;
				}
				rdataptr += 64;

				rdataptr = Write2LE( rdataptr, 16000 );
				trysend = 255; // Need fixed sized frames.
				r = hid_send_feature_report( hd, rdata, trysend );
				if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
				r = hid_get_feature_report( hd, rdata, trysend );
				if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
				if( rdata[1] != 0 )
				{
					fprintf( stderr, "Error writing block %d [%02x %d]\n", b, rdata[0], (char)rdata[1] );
					return -9;
				}
				printf( "." ); fflush( stdout );
				addy += 64;
			}
			fclose( f );

			printf( "\nWrote %d blocks\n", blocks );
			break;
		}
		case 2:
		{
			memset( rdata, 0xff, sizeof( rdata ) );
			rdataptr = rdata;
			*(rdataptr++) = 0xad; // Report ID.
			*(rdataptr++) = 0x95;
			trysend = 255; // Need fixed sized frames.
			r = hid_send_feature_report( hd, rdata, trysend );
			if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
			r = hid_get_feature_report( hd, rdata, trysend );
			if( r < 0 ) { fprintf( stderr, "Error reading request\n" ); return -1; };
			if( rdata[0] != 1 || rdata[1] != 0 )
			{
				fprintf( stderr, "Error erasing (%02x %d)\n", rdata[0], (char)rdata[1] );
				return -55;
			}

			printf( "Erase Complete\n" ); fflush( stdout );
			goto reinit;
		}
		case 3:
		{
			// BOOT
			memset( rdata, 0xff, sizeof( rdata ) );
			rdataptr = rdata;
			*(rdataptr++) = 0xad; // Report ID.
			*(rdataptr++) = 0x92;
			trysend = 255; // Need fixed sized frames.
			r = hid_send_feature_report( hd, rdata, trysend );
			if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
			r = hid_get_feature_report( hd, rdata, trysend );
			if( r < 0 ) { fprintf( stderr, "Error reading request\n" ); return -1; };
			printf( "Off Ok\n" ); fflush( stdout );
			usleep( 10000 );
			memset( rdata, 0xff, sizeof( rdata ) );
			rdataptr = rdata;
			*(rdataptr++) = 0xad; // Report ID.
			*(rdataptr++) = 0x91;
			trysend = 255; // Need fixed sized frames.
			r = hid_send_feature_report( hd, rdata, trysend );
			if( r < 0 ) { fprintf( stderr, "Error sending request\n" ); return -1; };
			r = hid_get_feature_report( hd, rdata, trysend );
			if( r < 0 ) { fprintf( stderr, "Error reading request\n" ); return -1; };
			printf( "On Ok\n" ); fflush( stdout );


			break;
		}
		}
	}
	return 0;
errorwarning:
	fprintf( stderr, "Usage: [tool] [READ address length [file|-]]/[WRITE address file]/[ERASE]\n" );

	return -5;
}

