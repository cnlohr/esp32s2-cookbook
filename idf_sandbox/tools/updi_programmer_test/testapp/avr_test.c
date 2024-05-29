#include <util/delay.h>


struct IOPORT
{
	volatile uint8_t DIR;
	volatile uint8_t DIRSET;
	volatile uint8_t DIRCLR;
	volatile uint8_t DIRGL;
	volatile uint8_t OUT;
	volatile uint8_t OUTSET;
	volatile uint8_t OUTCLR;
	volatile uint8_t OUTGL;
	volatile uint8_t IN;
	volatile uint8_t INTFLAGS;
	volatile uint8_t RES0;
	volatile uint8_t RES1;
	volatile uint8_t RES2;
	volatile uint8_t RES3;
	volatile uint8_t RES4;
	volatile uint8_t RES5;
	volatile uint8_t PIN0CTRL;
	volatile uint8_t PIN1CTRL;
	volatile uint8_t PIN2CTRL;
	volatile uint8_t PIN3CTRL;
	volatile uint8_t PIN4CTRL;
	volatile uint8_t PIN5CTRL;
	volatile uint8_t PIN6CTRL;
	volatile uint8_t PIN7CTRL;
};

volatile struct IOPORT * PORTC = (volatile struct IOPORT*)0x0440;

int main()
{
    PORTC->DIR |= 1;
	while(1)
	{
		PORTC->OUT |= 1;
		_delay_ms(500);
		PORTC->OUT &= ~1;
		_delay_ms(500);
	}
	return 0;
}


