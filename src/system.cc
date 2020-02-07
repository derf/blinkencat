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

#define BIT_WW _BV(PD5)
#define BIT_RED _BV(PB3)
#define BIT_GREEN _BV(PB4)
#define BIT_BLUE _BV(PB2)

#define PWM_RED OCR1A
#define PWM_GREEN OCR1B
#define PWM_BLUE OCR0A

/*
// cos
#define HSBTABLE_LEN 80
#define HSBTABLE_MAX (3*HSBTABLE_LEN)
uint8_t const hsbtable[80] PROGMEM = {
	254, 254, 253, 252, 250, 248, 245, 242, 238, 234, 229, 224, 219, 213, 207,
	201, 194, 188, 181, 174, 167, 160, 153, 146, 139, 133, 126, 119, 113, 106,
	100, 94, 88, 83, 78, 72, 67, 63, 58, 54, 50, 46, 43, 39, 36, 33, 30, 28, 25,
	23, 21, 19, 17, 16, 14, 13, 11, 10, 9, 8, 7, 6, 6, 5, 4, 4, 3, 3, 2, 2, 2,
	1, 1, 1, 1, 0, 0, 0, 0, 0
};
*/

/*
 * Linear fading
 * print([int(round(2**(np.log2(256) * (84-x) / 84) - 1)) for x in range(84)])
 */
#define HSBTABLE2_LEN 84
#define HSBTABLE2_MAX (3*HSBTABLE_LEN)
uint8_t const hsbtable2[84] PROGMEM = {
	254, 239, 223, 209, 196, 183, 171, 160, 150, 140, 131, 123, 115, 108, 101,
	94, 88, 82, 77, 72, 67, 63, 59, 55, 52, 48, 45, 42, 39, 37, 34, 32, 30, 28,
	26, 24, 23, 21, 20, 19, 17, 16, 15, 14, 13, 12, 11, 11, 10, 9, 8, 8, 7, 7,
	6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0,
	0, 0, 0, 0, 0
};

/*
 * Linear fading for non-powerconscious HSB
 * print([int(round(2**(np.log2(256) * (42-x) / 42) - 1)) for x in range(42)])
 */
#define HSBTABLE_LEN 42
#define HSBTABLE_MAX (6*HSBTABLE_LEN)
uint8_t const hsbtable[42] PROGMEM = {
	254, 223, 196, 171, 150, 131, 115, 101, 88, 77, 67, 59, 52, 45, 39, 34, 30,
	26, 23, 20, 17, 15, 13, 11, 10, 8, 7, 6, 5, 5, 4, 3, 3, 2, 2, 2, 1, 1, 1, 0,
	0, 0
};

uint8_t const energytable[12] PROGMEM = {
	2, // WARMWHITE
	3, // SLOWRGB
	9, // SUN
	3, // RED
	2, // GREEN
	2, // BLUE
	5, // YELLOW
	5, // MAGENTA
	4, // CYAN
	3, // FASTRGB
	1, // SLOWRGB2
	1  // FASTRGB2
};

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

void System::loop()
{
	uint8_t anim_step = 0;
	if (mode_changed) {
		mode_changed = 0;
		if ((mode == SLOWRGB) || (mode >= FASTRGB)) {
			PORTD &= ~BIT_WW;
			PORTB = 0;
			// 8 bit fast PWM on OC0A, OC1A, OC1B
			TCCR0A = _BV(COM0A1) | _BV(WGM01) | _BV(WGM00);
			TCCR0B = _BV(CS00);
			TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
			TCCR1B = _BV(WGM12) | _BV(CS00);

			// interrupt on Timer 0 overflow (31.25 kHz)
			TIMSK = _BV(TOIE0);
		} else if (mode == OFF) {
			TCCR0A = 0;
			TCCR0B = 0;
			TCCR1A = 0;
			TCCR1B = 0;
			TIMSK = 0;
		} else {
			TCCR0A = _BV(WGM01) | _BV(WGM00);
			TCCR0B = _BV(CS00);
			TCCR1A = 0;
			TCCR1B = 0;
			// interrupt on Timer 0 overflow (31.25 kHz)
			TIMSK = _BV(TOIE0);
		}
		switch (mode) {
			case OFF:
				PORTD &= ~BIT_WW;
				PORTB = 0;
				break;
			case WARMWHITE:
				PORTD |= BIT_WW;
				PORTB = 0;
				break;
			case SUN:
				PORTD |= BIT_WW;
				PORTB = BIT_RED | BIT_GREEN | BIT_BLUE;
				break;
			case RED:
				PORTD &= ~BIT_WW;
				PORTB = BIT_RED;
				break;
			case GREEN:
				PORTB = BIT_GREEN;
				break;
			case BLUE:
				PORTB = BIT_BLUE;
				break;
			case YELLOW:
				PORTB = BIT_RED | BIT_GREEN;
				break;
			case MAGENTA:
				PORTB = BIT_RED | BIT_BLUE;
				break;
			case CYAN:
				PORTB = BIT_GREEN | BIT_BLUE;
				break;
			default:
				break;
		}
	}

	if ((mode == FASTRGB) || (mode == FASTRGB2)) {
		anim_step = anim_step_fine;
	} else if ((mode == SLOWRGB) || (mode == SLOWRGB2)) {
		anim_step = anim_step_coarse;
	}

	if ((mode == FASTRGB) || (mode == SLOWRGB)) {
		if (anim_step < HSBTABLE_LEN) {
			PWM_GREEN = pgm_read_byte(&hsbtable[HSBTABLE_LEN - 1 - anim_step]);
		}
		else if (anim_step < 2*HSBTABLE_LEN) {
			PWM_RED = pgm_read_byte(&hsbtable[anim_step - HSBTABLE_LEN]);
		}
		else if (anim_step < 3*HSBTABLE_LEN) {
			PWM_BLUE = pgm_read_byte(&hsbtable[3*HSBTABLE_LEN - 1 - anim_step]);
		}
		else if (anim_step < 4*HSBTABLE_LEN) {
			PWM_GREEN = pgm_read_byte(&hsbtable[anim_step - 3*HSBTABLE_LEN]);
		}
		else if (anim_step < 5*HSBTABLE_LEN) {
			PWM_RED = pgm_read_byte(&hsbtable[5*HSBTABLE_LEN - 1 - anim_step]);
		}
		else if (anim_step < 6*HSBTABLE_LEN) {
			PWM_BLUE = pgm_read_byte(&hsbtable[anim_step - 5*HSBTABLE_LEN]);
		}
	} else if ((mode == FASTRGB2) || (mode == SLOWRGB2)) {
		if (anim_step < HSBTABLE2_LEN) {
			PWM_RED = pgm_read_byte(&hsbtable2[anim_step]);
			PWM_GREEN = pgm_read_byte(&hsbtable2[HSBTABLE2_LEN - 1 - anim_step]);
		}
		else if (anim_step < 2*HSBTABLE2_LEN) {
			PWM_GREEN = pgm_read_byte(&hsbtable2[anim_step - HSBTABLE2_LEN]);
			PWM_BLUE = pgm_read_byte(&hsbtable2[2*HSBTABLE2_LEN - 1 - anim_step]);
		}
		else if (anim_step < 3*HSBTABLE2_LEN) {
			PWM_RED = pgm_read_byte(&hsbtable2[3*HSBTABLE2_LEN - 1 - anim_step]);
			PWM_BLUE = pgm_read_byte(&hsbtable2[anim_step - 2*HSBTABLE2_LEN]);
		}
	}
	if ((mode == SLOWRGB) || (mode >= FASTRGB)) {
		if (OCR0A)
			TCCR0A |= _BV(COM0A1);
		else
			TCCR0A &= ~_BV(COM0A1);
		if (OCR1A)
			TCCR1A |= _BV(COM1A1);
		else
			TCCR1A &= ~_BV(COM1A1);
		if (OCR1B)
			TCCR1A |= _BV(COM1B1);
		else
			TCCR1A &= ~_BV(COM1B1);
	}

	if (mode == OFF) {
		sleep();
	} else {
		if (tick_18s) {
			tick_18s = 0;
			energy_j -= pgm_read_byte(&energytable[mode - 1]);
		}
		if (energy_j < 0) {
			mode_changed = 1;
			energy_j = -1;

			PORTD &= ~BIT_WW;
			PORTB = BIT_RED;
			_delay_ms(50);

			mode = OFF;
		}
		idle();
	}
}

void System::next_mode(void)
{
	if (!btn_debounce) {
		mode = (BCMode)((mode + 1) % MODE_ENUM_MAX);
		mode_changed = 1;
	}
}

void System::debounce_start(void)
{
	if (!btn_debounce) {
		btn_debounce = 1;
		wdt_reset();
		WDTCSR = _BV(WDE) | _BV(WDCE);
		WDTCSR = _BV(WDIE) | _BV(WDP2);
	}
}

void System::debounce_done(void)
{
	btn_debounce = 0;
	wdt_disable();
	// long press? -> turn off
	if (!(PIND & _BV(PD2))) {
		mode_changed = 1;
		blinkencat.mode = OFF;
	}
}

ISR(WDT_OVERFLOW_vect)
{
	blinkencat.debounce_done();
}

ISR(TIMER0_OVF_vect)
{
	static uint8_t slowdown = 0;
	static uint8_t slowdown2 = 0;
	static uint8_t slowdown3 = 0;
	if (++slowdown == 10) {
		slowdown = 0;
		// 3.125 kHz
		if (++blinkencat.anim_step_fine == HSBTABLE_MAX) {
			blinkencat.anim_step_fine = 0;
			if (++blinkencat.anim_step_coarse == HSBTABLE_MAX) {
				blinkencat.anim_step_coarse = 0;
			}
		}
		if (++slowdown2 == 255) {
			slowdown2 = 0;
			// 12.25 Hz
			if (++slowdown3 == 221) {
				slowdown3 = 0;
				// ~55.5 mHz == (1/18) Hz
				blinkencat.tick_18s = 1;
			}
		}
	}
}

ISR(PCINT2_vect)
{
	if (!(PIND & _BV(PD2))) {
		blinkencat.next_mode();
	}
	if (PIND & _BV(PD3)) {
		blinkencat.is_charging = 1;
		blinkencat.setEnergyFull();
		PORTD |= _BV(PD1);
	} else {
		blinkencat.is_charging = 0;
		PORTD &= ~_BV(PD1);
	}
	blinkencat.debounce_start();
}
