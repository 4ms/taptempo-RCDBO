#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdarg.h>
#include <ctype.h>



/** SETTINGS **/
#define PW 10



/** PINS **/

//clock input jack
#define CLOCK_IN_pin PD2
#define CLOCK_IN_init DDRD &= ~(1<<CLOCK_IN_pin)
#define CLOCK_IN (PIND & (1<<CLOCK_IN_pin))

//clock input LED
#define CLOCK_IN_LED_pin PD3
#define CLOCK_IN_LED_init DDRD |=(1<<CLOCK_IN_LED_pin)
#define CLOCK_IN_LED_PORT PORTD

//clock output jack, tied to clock output LED
#define CLOCK_OUT_pin PB0
#define CLOCK_OUT_init DDRB |= (1<<CLOCK_OUT_pin)
#define CLOCK_OUT_PORT PORTB

//tap button (tempo/single)
#define TAP_pin PB1
#define TAP_init DDRB &= ~(1<<TAP_pin)
#define TAP_BUTTON (!(PINB & (1<<TAP_pin)))

//panel switches wired to this pcb
#define SWITCH_IN_MASK 0b00111100
#define SWITCH_IN_pullup PORTB |= (SWITCH_IN_MASK)
#define SWITCH_IN_PIN PINB
#define SWITCH_IN_init DDRB &= ~(SWITCH_IN_MASK)

#define SWITCH_IN_MASK2 0b11000000
#define SWITCH_IN_pullup2 PORTD |= (SWITCH_IN_MASK2)
#define SWITCH_IN_PIN2 PIND
#define SWITCH_IN_init2 DDRD &= ~(SWITCH_IN_MASK2)

//output header pins connects to main clock module's jumpers (labeled "3" - "7")
#define SWITCH3 PC2
#define SWITCH4 PC3
#define SWITCH5 PC4
#define SWITCH6 PC5
#define SWITCH_OUT_MASK1 (1<<SWITCH3) | (1<<SWITCH4) | (1<<SWITCH5) | (1<<SWITCH6)
#define SWITCH_OUT_PORT1 PORTC
#define SWITCH_OUT_init1 DDRC |= (SWITCH_OUT_MASK1)


#define SWITCH7 PD5
#define SWITCH8 PD4
#define SWITCH_OUT_MASK2 (1<<SWITCH7) | (1<<SWITCH8)
#define SWITCH_OUT_PORT2 PORTD
#define SWITCH_OUT_init2 DDRD |= (SWITCH_OUT_MASK2)


#define MODE_SWITCH1_PIN PC0
#define MODE_SWITCH2_PIN PC1
#define MODE_SWITCH_init DDRC &= ~((1<<MODE_SWITCH1_PIN) | (1<<MODE_SWITCH2_PIN))
#define MODE_SWITCH1 (PINC & (1<<MODE_SWITCH1_PIN))
#define MODE_SWITCH2 (PINC & (1<<MODE_SWITCH2_PIN))



/** MACROS **/

#define ALLON(p,x) p &= ~(x)
#define ALLOFF(p,x) p |= (x)
#define ON(p,x) p &= ~(1<<(x))
#define OFF(p,x) p |= (1<<(x))



/** TIMER **/
#define TMROFFSET 10

volatile long tmr;

SIGNAL (SIG_OVERFLOW0){
	tmr+=1;
	TCNT0=TMROFFSET; // Re-init timer
}

long gettmr(void){
	long result;
	cli();
	result = tmr;
	sei();
	return result;
}

void inittimer(void){
	TCCR0A=0;
	TCCR0B=(1<<CS00) | (1<<CS01);		// Prescaler CK/64 @ 16MHz is 250kHz
	TCNT0=TMROFFSET;
	TIMSK0=(1<<TOIE0); 					// Enable timer overflow interrupt
	sei();
}



/** MAIN **/


int main(void){

	unsigned char clock_up=0,clock_down=0;
	unsigned char clock_just_struck=0;
	unsigned char tap_up=0,tap_down=0;
	long now=0,period=0,last=0,o0=0,time_of_tap=0;



	CLOCK_IN_init;
	CLOCK_IN_LED_init;
	CLOCK_OUT_init;
	TAP_init;

	SWITCH_IN_init;
	SWITCH_IN_init2;
	SWITCH_IN_pullup;
	SWITCH_IN_pullup2;
	SWITCH_OUT_init1;
	SWITCH_OUT_init2;

	MODE_SWITCH_init;


	inittimer();

	while(1){


		now=gettmr();


		if (CLOCK_IN) {
			if (!clock_up){
				//Set the clock_up flag so that we only do this little block of code the first time 
				//we notice the CLOCK_IN pin has gone high (rising edge)
				clock_up=1;
				clock_just_struck=1;

				//Also, we reset the clock_down flag so that we will execute the "clock down" block
				//of code the next time we notice the CLOCK_IN pin is low
				clock_down=0;

				ON(CLOCK_IN_LED_PORT,CLOCK_IN_LED_pin);
			}
		}
		else {
			if (!clock_down){
				clock_up=0;
				clock_down=1;
				OFF(CLOCK_IN_LED_PORT,CLOCK_IN_LED_pin);
			}
		}

// The MODE SWITCH is a 3-position SPDT switch (ON-OFF-ON)
// The center terminal (pole) is tied to 5V,
// and the two side terminals (throws) are run to the chip:
// SWITCH1 is high when switch is down
// SWITCH2 is high when switch is up
// The mode determines whether the internal clock is running, and whether the button
// sets the tempo (tap tempo) or if it just outputs a pulse on the clock output jack

// Here's how the three modes line up to the three switch positions
// Switch up: SWITCH1 low, SWITCH2 high: button taps directly add to running clock
// Switch center: SWITCH1 low, SWITCH2 low: button taps set tap tempo, clock is running
// Switch down: SWITCH1 high, SWITCH2 low: button taps output directly, clock is not outputting


		if (TAP_BUTTON || clock_just_struck){
			clock_just_struck=0;
			time_of_tap=now;
			if (!tap_up){
				tap_up=1;
				tap_down=0;

				if (!MODE_SWITCH1 && !MODE_SWITCH2){
					//center position is tap tempo mode
					period=now-last;
					last=now;
					o0=now;
				} //else {
					//the other positions mean direct tap mode
					ON(CLOCK_OUT_PORT,CLOCK_OUT_pin);
			//	}
			}

		}else if (!TAP_BUTTON){ //TAP_BUTTON is up

			// Don't even process a TAP_BUTTON up event
			// unless it's been registered as being down for a while
			if ((now-time_of_tap)>10){ 
				if (!tap_down){
					tap_down=1;
					tap_up=0;

				//	if (MODE_SWITCH1 || MODE_SWITCH2){ 
						//either side position is direct tap mode
						OFF(CLOCK_OUT_PORT,CLOCK_OUT_pin);
				//	}
				}
			}
		}

		if (now==(o0+(period>>1))) {
			if (!MODE_SWITCH1)
				OFF(CLOCK_OUT_PORT,CLOCK_OUT_pin);
		}

		if (now==(o0+period)) {
			o0=now;
			if (!MODE_SWITCH1) //Run mode
				ON(CLOCK_OUT_PORT,CLOCK_OUT_pin);
		}





		if (SWITCH_IN_PIN & (1<<2)) SWITCH_OUT_PORT1 |= (1<<SWITCH3);
		 else SWITCH_OUT_PORT1 &= ~(1<<SWITCH3);

		if (SWITCH_IN_PIN & (1<<3)) SWITCH_OUT_PORT1 |= (1<<SWITCH4);
		 else SWITCH_OUT_PORT1 &= ~(1<<SWITCH4);

		if (SWITCH_IN_PIN & (1<<4)) SWITCH_OUT_PORT1 |= (1<<SWITCH5);
		 else SWITCH_OUT_PORT1 &= ~(1<<SWITCH5);

		if (SWITCH_IN_PIN & (1<<5)) SWITCH_OUT_PORT1 |= (1<<SWITCH6);
		 else SWITCH_OUT_PORT1 &= ~(1<<SWITCH6);

		if (SWITCH_IN_PIN2 & (1<<6)) SWITCH_OUT_PORT2 |= (1<<SWITCH7);
		 else SWITCH_OUT_PORT2 &= ~(1<<SWITCH7);

		if (SWITCH_IN_PIN2 & (1<<7)) SWITCH_OUT_PORT2 |= (1<<SWITCH8);
		 else SWITCH_OUT_PORT2 &= ~(1<<SWITCH8);

	}	//endless loop

	return(1);
}
