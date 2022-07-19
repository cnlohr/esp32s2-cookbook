#define CNFG_IMPLEMENTATION

#include "rawdraw_sf.h"

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

#include <pthread.h>

float buffer[1024];
int head = 0;
int tail = 0;

void * vmt( void * v)
{
	do
	{
		char *line =0;
		size_t len = 0;
		ssize_t lineSize = 0;
		lineSize = getline(&line, &len, stdin);
		printf( "%s\n", line );
		int c;
		char * nextstart;
		nextstart = 0;
		for( c = 0; line[c]; c++ )
		{
			if( line[c] == ',' )
			{
				nextstart = line + c + 1;
				float f = atof( nextstart );
				head = (head+1)%1024;
				buffer[head] = f;
				break;
			}
		}
	} while(1);
}

pthread_t pt;

int main()
{
	CNFGSetup( "Example Graph App", 1024, 768 );
	
	int i;
	for( i =0 ; i< 1024; i++ )buffer[i] = 1e30;
	
	pthread_create(&pt, 0, vmt, 0 );

	while(CNFGHandleInput())
	{
		short w, h;

		CNFGBGColor = 0x000080ff; //Dark Blue Background

		CNFGClearFrame();
		CNFGGetDimensions( &w, &h );

		CNFGColor( 0xffffffff ); 
		int x;
		float minv = 1e20;
		float maxv = -1e20;
		for( x = 0; x < 1024; x++ )
		{
			float v0 = buffer[(x+head)%1024];
			if( v0 > 1e20 ) continue;
			if( v0 < minv ) minv = v0;
			if( v0 > maxv ) maxv = v0;
		}
		
		for( x = 0; x < 1024; x++ )
		{
			float v0 = buffer[(x+head)%1024];
			float v1 = buffer[(x+head+1)%1024];
			v0 = h - (v0 - minv) / (maxv - minv ) * (h-2) -1;
			v1 = h - (v1 - minv) / (maxv - minv ) * ( h-2) -1;
			CNFGTackSegment( x, v0, x+1, v1 );
		
		}		


		CNFGSwapBuffers();		
	}
}
