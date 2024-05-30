#include <util/delay.h>

// So it can be used with old avr-libc's.
#define _AVR_IO_H_
#include <avr/sfr_defs.h>
#include "iotn416.h"

int main()
{
	// Configure / enable 32.768k crystal
	

    PORTC.DIRSET = 1;
    PORTA.DIRSET = 1<<4;
	while(1)
	{
		PORTC.OUTTGL = 1;
		PORTA.OUTTGL = 1<<4;
	}
	return 0;
}


