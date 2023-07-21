#include <stdio.h>

#define CNFG_IMPLEMENTATION

#include "rawdraw_sf.h"


/* Example
gcc -o linegraph linegraph.c  -lX11 && stty -F /dev/ttyUSB0 115200 -echo raw | cat /dev/ttyUSB0 | ./linegraph 

*/

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

int main( int argc, char ** argv )
{
	// First parameter (optional) is how many fields to skip.
	// Second to parameters (optional) = min, max force
	int offset = 0;
	float mm_min, mm_max;
	int force_minmax = 0;
	if( argc > 1 )
	{
		offset = atoi( argv[1] );
	}
	
	if( argc > 3 )
	{
		force_minmax = 1;
		mm_min = atof( argv[2] );
		mm_max = atof( argv[3] );
	}
	
	CNFGSetup( "per-line grapher", 1024, 768 );
	while(CNFGHandleInput())
	{
		CNFGBGColor = 0x000080ff; //Dark Blue Background

		short w, h;
		CNFGClearFrame();
		CNFGGetDimensions( &w, &h );


		char line[8192];
		char * fgs = fgets(line, sizeof(line), stdin);
		if( fgs == 0 )
		{
			break;
		}
		
		int field = 0;
		char * fieldstart = 0;
		double fields[8192];
		for( char * c = line; ; c++ )
		{
			if( *c == ' ' || *c == '\t' || *c == ':' || *c == 0 || *c == '\r' || *c == '\n' )
			{
				if( fieldstart )
				{
					if( c - fieldstart >= 1 )
					{
						if( field-offset >= 0 )
							fields[field-offset] = atof( fieldstart );
						field++;
					}
					fieldstart = 0;
				}
				if( !*c || *c == '\r' || *c == '\n' ) break;
			}
			else
			{
				if( !fieldstart ) fieldstart = c;
			}
		}

		//printf( "FIELDS: %d [%s %s]\n", field, line, fgs );

		field-=offset;
		if( field <= 0 ) continue;
		int fdiv = w/field;

		int i;
		double fdmin = 1e20;
		double fdmax = -1e20;
		if( force_minmax )
		{
			fdmin = mm_min;
			fdmax = mm_max; 
		}
		else
		{
			for( i = 0; i < field; i++ )
			{
				if( fields[i] < fdmin ) fdmin = fields[i];
				if( fields[i] > fdmax ) fdmax = fields[i];
			}
		}
		//Change color to white.
		CNFGColor( 0xffffffff ); 
		
		fdmin--;
		fdmax++;
		printf( "%f %f %f\n", fdmin, fdmax, fdmax-fdmin );
		for( i = 1; i < field; i++ )
		{
			double last = h - 1 - (fields[i-1] - fdmin)/(fdmax - fdmin)*(h-1);
			double next = h - 1 - (fields[i] - fdmin)/(fdmax - fdmin)*(h-1);
			CNFGTackSegment( fdiv*i, last, fdiv*(i+1), next );	
		}

		for( i = 0; i < field; i++ )
		{
			double now = h - 1 - (fields[i] - fdmin)/(fdmax - fdmin)*(h-1);
			CNFGTackSegment( fdiv*(i+1)+5, now+5, fdiv*(i+1)-5, now+5 );
			CNFGTackSegment( fdiv*(i+1)-5, now+5, fdiv*(i+1)-5, now-5 );
			CNFGTackSegment( fdiv*(i+1)-5, now-5, fdiv*(i+1)+5, now-5 );	
			CNFGTackSegment( fdiv*(i+1)+5, now-5, fdiv*(i+1)+5, now+5 );	
		}

		//Display the image and wait for time to display next frame.
		CNFGSwapBuffers();		
	}
}
