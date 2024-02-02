#include <stdio.h>
#include <stdint.h>
#include "../siggen.h"

int main()
{
	SigSetupTest();

	int frame;
	for( frame = 0; frame < 9000; frame++ )
	{
		uint32_t ltime = frame;
		uint32_t clock = ltime * 2400;
		uint32_t fo = SigGen( clock, 10000 );
		printf( "%d, %d\n", frame, fo );
	}
}

