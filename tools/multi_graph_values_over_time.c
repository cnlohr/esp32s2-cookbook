#define CNFG_IMPLEMENTATION

#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

#include <pthread.h>

#define MAX_INS 10

float buffer[16384][MAX_INS];
int head = 0;
int tail = 0;

int do_fixed = 0;
float fixed_min = 0; 
float fixed_max = 0; 

void * vmt( void * v)
{
	int headnext = head + 1;
	do
	{
		char *line =0;
		size_t len = 0;
		ssize_t lineSize = 0;
		lineSize = getline(&line, &len, stdin);
		printf( "%s", line );
		if( lineSize < 1 ) exit( 0 );
		int c;
		char * nextstart;
		nextstart = line;
		int last_white = 1;
		int npl = 0;
		for( c = 0; c < lineSize; c++ )
		{
			int is_white = line[c] == ',' || line[c] == ' ' || line[c] == '\t' || line[c] == '\n';
			if( (is_white && !last_white) )
			{
				float f = atof( nextstart );
				if( npl < MAX_INS && nextstart[0] <= '9' && nextstart[0] >= '0' )
					buffer[head][npl++] = f;
				nextstart = line + c + 1;
			}
			last_white = is_white;
		}
		head = headnext;
		headnext = (head+1)%16384;
		for( npl = 0; npl < MAX_INS; npl++ )
			buffer[headnext][npl] = 0.0/0.0;
	} while(1);
}

pthread_t pt;

int main( int argc, char ** argv )
{
	CNFGSetup( "Example Graph App", 1024, 768 );
	
	int i;
	int n;
	for( n = 0; n < MAX_INS; n++ )
		for( i = 0; i < 16384; i++ ) buffer[i][n] = 0.0/0.0;
	
	if( argc == 1 )
	{
		// Ok
	}
	else if( argc == 3 )
	{
		do_fixed = 1;
		fixed_min = atof( argv[1] );
		fixed_max = atof( argv[2] );
	}
	else
	{
		fprintf( stderr, "Error: Must be 0 or 2 parameters.  If 2 parameters, force defines min/max\n" );
		return -9;
	}

	pthread_create(&pt, 0, vmt, 0 );

	while(CNFGHandleInput())
	{
		short w, h;

		CNFGBGColor = 0x000080ff; //Dark Blue Background

		CNFGClearFrame();
		CNFGGetDimensions( &w, &h );

		CNFGColor( 0xffffffff ); 
		int x;

		for( n = 0; n < MAX_INS; n++ )
		{
			float minv = 1e20;
			float maxv = -1e20;
			if( do_fixed )
			{
				minv = fixed_min;
				maxv = fixed_max;
			}
			else
			{
				for( x = 0; x < w; x++ )
				{
					float v0 = buffer[(head-x+16384)%16384][n];
					if( v0 != v0 ) continue;
					if( v0 < minv ) minv = v0;
					if( v0 > maxv ) maxv = v0;
				}
			}
			CNFGPenX = 20;
			CNFGPenY = 20 + n * 40;
			char cts2[1024];
			sprintf( cts2, "%f\n%f\n%f", minv, maxv, maxv-minv );

			// Make sure not NAN
			if( buffer[(head-1+16384)%16384][n] == buffer[(head-1+16384)%16384][n] )
				CNFGDrawText( cts2, 2 );

			for( x = 0; x < w; x++ )
			{
				float v0 = buffer[(head-x+16384)%16384][n];
				float v1 = buffer[(head-x+16384+1)%16384][n];
				float v0i = v0;
				float v1i = v1;
				if( v0 < minv ) v0 = minv;
				if( v1 < minv ) v1 = minv;
				if( v0 > maxv ) v0 = maxv;
				if( v1 > maxv ) v1 = maxv;
				if( v0i != v0 || v1i != v1 )
					CNFGColor( 0xff0000ff );
				else
					CNFGColor( 0xffffffff );
				v0 = h - (v0 - minv) / (maxv - minv ) * (h-2) -1;
				v1 = h - (v1 - minv) / (maxv - minv ) * ( h-2) -1;
				CNFGTackSegment( x+1, v0, x, v1 );
			}		
		}

		CNFGSwapBuffers();		
	}
}
