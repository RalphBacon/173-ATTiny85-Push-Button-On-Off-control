// ATMEL ATTINY 25/45/85 / ARDUINO
// Pin 1 is /RESET
//
//                  +-\/-+
//            PB5  1|o   |8  Vcc
//            PB3  2|    |7  PB2
//            PB4  3|    |6  PB1
//            GND  4|    |5  PB0
//                  +----+

#include "Arduino.h"
#include <avr/interrupt.h>
#include <SendOnlySoftwareSerial.h>

// Interrupt pin PB2 aka PCINT2 aka physicl pin 7.
// This is the only pin we can configure for HIGH, LOW all
// others are on PIN_CHANGE
#define INT_PIN PB2

// Output LOW pin to keep MOSfET running
#define PWR_PIN PB4

// Power ON LED pin (will flash on shutdown) pin 6
#define PWR_LED PB1

// KILL interrupt for external µC
#define KILL PB3

// ISR handled variables
volatile bool togglePowerRequest = false;
bool shutDownAllowed = false;

// State of output ON/OFF
bool powerIsOn = false;
bool shutDownInProgress = false;

// When did we switch ON (so we can delay switching off)
unsigned long millisOnTime = 0;

// How long to hold down button to force turn off (mS)
#define KILL_TIME 5000

// Minimum time before shutdown requests accepted (mS)
#define MIN_ON_TIME 5000

// When did we press and hold the OFF button
unsigned long millisOffTime = 0;

// Forward declaration(s)
void powerDownLedFlash();
void shutDownPower();
//void btnISR();

// Serial comms - plug in a USB-to-Serial device to PB0 (pin 5)
SendOnlySoftwareSerial Serial(PB0);

//==================================
//SETUP    SETUP     SETUP     SETUP
//==================================
void setup()
{
	Serial.begin(9600);
	Serial.println("Setup starts");

	// Keep the KILL command a WEAK HIGH until we need it
	pinMode(KILL, INPUT_PULLUP);

	// Keep the MOSFET switched OFF initially
	//digitalWrite(PWR_PIN, HIGH);
	digitalWrite(PWR_PIN, LOW);
	pinMode(PWR_PIN, OUTPUT);

	// Power ON Led
	digitalWrite(PWR_LED, LOW);
	pinMode(PWR_LED, OUTPUT);

	// Interrupt pin when we press power pin again
	pinMode(INT_PIN, INPUT_PULLUP);
	// The Arduino preferred syntax of attaching interrupt not possible
	// attachInterupt(digitalPinToInterrupt(INT_PIN), PWR_PIN_ISR, LOW);

	// We want only on FALLING (not LOW, if possible) set bits 1:0 to zero
	// MCUCR &= ~(_BV(ISC01) | _BV(ISC00)); // LOW
	// GIFR |= bit(INTF0);  // clear any pending INT0
	// GIMSK |= bit(INT0);  // enable INT0
	// attachInterrupt(0, btnISR, FALLING);
	MCUCR = (MCUCR & ~((1 << ISC00) | (1 << ISC01))) | 2 << ISC00;

	// Equivalent to 1 << (6) That is, set bit 6 for ext interrupts
	// in the Gene3ral Interrupts Mask Register
	// GIMSK |= _BV(INT0); equivalent to
	//GIMSK |= 0b01000000;
	GIMSK |= (1 << INT0);

	/* You can also turn on pin CHANGE interrupts on selected pin(s) by
	 * setting the pin in the bit pattern:
	 *
	 * // turn on interrupts on pins PB1, PB2, & PB4
	 * PCMSK = 0b00010110;
	 */

	// Time to power up
	togglePowerRequest = true;
}

//==============================
// LOOP    LOOP    LOOP     LOOP
//==============================
void loop()
{
	//; Only after a few seconds do we allow shutdown requests
	if (millis() >= millisOnTime + MIN_ON_TIME && powerIsOn)
		if (!shutDownAllowed) {
			shutDownAllowed = true;
			Serial.println("Shutdown requests now accepted.");
		}

	// Button was pressed and no shut down already running
	if (togglePowerRequest && !shutDownInProgress) {
		// Is the power currently ON?
		if (powerIsOn) {
			// And we can switch off after initial delay period
			if (shutDownAllowed) {
				Serial.println("Shut Down Request Received.");
				shutDownInProgress = true;

				// Send the 100mS KILL command to external processor
				digitalWrite(KILL, LOW);
				pinMode(KILL, OUTPUT);
				Serial.println("Sent SHUTDOWN message to ext. µC");
				delay(10);

				// Now we wait for confirmation from ext. µC
				pinMode(KILL, INPUT_PULLUP);
				digitalWrite(KILL, HIGH);

				// Kill INT0 ext interrupt as we don't need it again
				// until shutdown completed (when we can power up again)
				GIMSK = 0b00000000;
				// detachInterrupt(0);
			}
		}
		else {
			// Power pin to MOSFET gate (LOW to turn on P-Channel MOSFET)
			//digitalWrite(PWR_PIN, LOW);
			digitalWrite(PWR_PIN, HIGH);

			// Turn on Power LED pin to indicate we are running
			digitalWrite(PWR_LED, HIGH);

			// Power is ON
			Serial.println("Power is ON.");

			// Accept shutdown requests after 5 seconds
			Serial.println("Shut down requests not yet accepted.");
			millisOnTime = millis();

			// Set the flag that power is now ON
			powerIsOn = true;
		}

		// Clear the button press state
		togglePowerRequest = false;
	}

	// IF we are shutting down do not accept further presses
	// and wait for confirmation from external µC before killing power
	if (shutDownInProgress) {

		// Flash the power LED to indicate shutdown request
		powerDownLedFlash();

		// If we get the KILL command from external µC shutdown power
		if (digitalRead(KILL) == LOW) {
			// External µC has confirmed KILL power
			Serial.println("KILL command received");

			// shut off the power
			shutDownPower();
		}

		// If the power switch is held down long enough KILL anyway
		if (digitalRead(INT_PIN) == HIGH) {
			// Button released so reset start time
			millisOffTime = 0;
		} else {

			// If we hanve't captured start of long press do it now
			if (millisOffTime == 0) {
				millisOffTime = millis();
			}

			// Button pressed long enough for KILL power?
			if (millis() > millisOffTime + KILL_TIME) {
				Serial.println("EMERGENCY KILL detected");
				delay(2000); // debug only

				// shut off the power (instant off)
				shutDownPower();
			}
		}
	}
}

// Standard interrupt vector (only on LOW / FALLING)
ISR(INT0_vect) {
	// Keep track of when we entered this routine
	static volatile unsigned long oldMillis = 0;

	Serial.println("ISR");

	//Switch has closed. Check for debounce.10mS should be OK
	if (millis() > oldMillis + 10) {
		togglePowerRequest = true;
		Serial.println("ISR OK");
		oldMillis = millis();
	}
}

//void btnISR() {
//	// Keep track of when we entered this routine
//	static volatile unsigned long oldMillis = 0;
//
//	Serial.println("btnISR");
//
//	//Switch has closed. Check for debounce.10mS should be OK
//	if (millis() > oldMillis + 10) {
//		togglePowerRequest = true;
//		Serial.println("btnISR OK");
//		oldMillis = millis();
//	}
//}

// Flash the POWER ON LED to indicate shutting down. this just toggles the LED
// on and off every X milliseconds. A nice indication that we are shutting down
void powerDownLedFlash() {
	static unsigned long flashMillis = millis();
	if (powerIsOn) {
		if (millis() > flashMillis + 250) {
			digitalWrite(PWR_LED, !digitalRead(PWR_LED));
			flashMillis = millis();
		}
	}
}

//Common shutdown routine
void shutDownPower() {
	// Reset all flags to initial state
	powerIsOn = false;
	shutDownAllowed = false;
	shutDownInProgress = false;
	millisOffTime = 0;

	// Wait for button to be released or we will start up again!
	while (digitalRead(INT_PIN) == LOW) {
		Serial.println("Waiting for button release");
		delay(10);
	}

	// Re-enable external interrupts so we can turn on power again
	// GIMSK = 0b01000000;
	//attachInterrupt(0, btnISR, FALLING);

	// Turn off indicator LED and MOSFET
	//digitalWrite(PWR_PIN, HIGH);
	Serial.println("Power is OFF.");
	digitalWrite(PWR_PIN, LOW);
	digitalWrite(PWR_LED, LOW);
}
