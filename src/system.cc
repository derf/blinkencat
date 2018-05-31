#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdlib.h>

// Debian's avr-libc lacks the ATTiny2313A WDTCSR definition
#define WDTCSR  _SFR_IO8(0x21)

#include "system.h"

/*
 * Charge Status IN        -> PD3 (PCINT14)
 * Piezo IN                -X
 * Charge Status LED OUT   -> PD1
 * Vcc                     -> VCC
 * GND                     -> GND
 * Warm White OUT          -> PD5 (OC0B)
 * Blue OUT                -> PB2 (OC0A)
 * Button IN               -> PD2 (PCINT13)
 * Red OUT                 -> PB3 (OC1A)
 * Green OUT               -> PB4 (OC1B)
 *
 */

System blinkencat;

void System::initialize()
{

	// disable analog comparator
	ACSR |= _BV(ACD);

	// disable unused modules
	PRR |= _BV(PRUSI) | _BV(PRUSART);

	wdt_disable();

	DDRA = 0;
	DDRB = _BV(DDB2) | _BV(DDB3) | _BV(DDB4);
	DDRD = _BV(DDD1) | _BV(DDD5);
	PORTD = _BV(PD2);

	/*
	 * Pin change interrupts on PD2 and PD3.
	 * They are also used to wake the chip from sleep, hence we can't use
	 * INT0/INT1 directly. For those, only a level interrupt is detected
	 * asynchronously.
	 */

	PCMSK2 = _BV(PCINT13) | _BV(PCINT14);
	GIMSK = _BV(PCIE2);

	sei();
}

void System::idle()
{
	MCUCR &= ~_BV(SM0);
	MCUCR |= _BV(SE);
	asm("sleep");
	MCUCR &= ~_BV(SE);
}

void System::sleep()
{
	MCUCR |= _BV(SM0) | _BV(SE);
	asm("sleep");
	MCUCR &= ~_BV(SE);
}

void System::set_outputs()
{
	if (warmwhite) {
		PORTD |= _BV(PD5);
	} else {
		PORTD &= ~_BV(PD5);
	}
	if (red) {
		PORTB |= _BV(PB3);
	} else {
		PORTB &= ~_BV(PB3);
	}
	if (green) {
		PORTB |= _BV(PB4);
	} else {
		PORTB &= ~_BV(PB4);
	}
	if (blue) {
		PORTB |= _BV(PB2);
	} else {
		PORTB &= ~_BV(PB2);
	}
}

void System::loop()
{
	switch (mode) {
		case OFF:
			warmwhite = red = green = blue = 0;
			break;
		case WARMWHITE:
			warmwhite = 255;
			red = green = blue = 0;
			break;
		case RED:
			red = 255;
			warmwhite = green = blue = 0;
			break;
		case GREEN:
			green = 255;
			warmwhite = red = blue = 0;
			break;
		case BLUE:
			blue = 255;
			warmwhite = red = green = 0;
			break;
		case YELLOW:
			red = green = 255;
			warmwhite = blue = 0;
			break;
		case MAGENTA:
			red = blue = 255;
			warmwhite = green = 0;
			break;
		case CYAN:
			green = blue = 255;
			warmwhite = red = 0;
			break;
		case SUN:
			warmwhite = red = green = blue = 255;
			break;
		default:
			break;
	}
	set_outputs();

	if (mode == OFF && !btn_debounce) {
		sleep();
	} else {
		idle();
	}
}

void System::next_mode(void)
{
	if (!btn_debounce) {
		mode = (BCMode)((mode + 1) % MODE_ENUM_MAX);
	}
}

void System::debounce_start(void)
{
	if (!btn_debounce) {
		btn_debounce = 1;
		wdt_reset();
		WDTCSR = _BV(WDE) | _BV(WDCE);
		WDTCSR = _BV(WDIE) | _BV(WDP1) | _BV(WDP0);
	}
}

void System::debounce_done(void)
{
	btn_debounce = 0;
	wdt_disable();
}

ISR(WDT_OVERFLOW_vect)
{
	blinkencat.debounce_done();
}

ISR(PCINT2_vect)
{
	if (!(PIND & _BV(PD2))) {
		blinkencat.next_mode();
	}
	if (PIND & _BV(PD3)) {
		blinkencat.is_charging = 1;
		PORTD |= _BV(PD1);
	} else {
		blinkencat.is_charging = 0;
		PORTD &= ~_BV(PD1);
	}
	blinkencat.debounce_start();
}
