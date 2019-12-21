/*
 * Demo sketch for the ATTiny85 on/off/reset project
 *
 * This sketch waits for a request to shutdown and confirms
 * it is ready for power to be removed back to originator
 */
#include "Arduino.h"

// The single dual purpose pin to accept shutdown request and
// confirm back that OK to kill power. On UNO can be pin 2 or 3.
#define KILL 2

// Flag to indicate shutdown has been requested
volatile bool shutDownRequestReceived = false;

// Forward delcarartion
void shutDownISR();
void beep();

//==================================
//SETUP    SETUP     SETUP     SETUP
//==================================
void setup()
{
	Serial.begin(9600);
	Serial.println("Setup started.");

	// Interrupt pins must be INPUT and in this case held high
	pinMode(KILL, INPUT_PULLUP);

	// Intially we attach an interrupt on that pin for KILL request
	attachInterrupt(digitalPinToInterrupt(KILL), shutDownISR, FALLING);

	beep();
	Serial.println("Setup completed.");
}

//==============================
// LOOP    LOOP    LOOP     LOOP
//==============================
void loop()
{
	// Shutdown aka KILL request?
	if (shutDownRequestReceived) {

		// Stop using an interrupt as we want to use it as an output pin now
		detachInterrupt(digitalPinToInterrupt(KILL));

		Serial.println("Housekeeping tasks in progress");

		// We are shutting down
		for (auto cnt = 0; cnt < 6; cnt++) {
			digitalWrite(11, !digitalRead(11));
			delay(250);
		}

		// Wait a minumum of 100mS then respond
		delay(3000); // debug demo
		digitalWrite(KILL, LOW);
		pinMode(KILL, OUTPUT);

		Serial.println("KILL confirmation sent");
		shutDownRequestReceived = false;
	}

	delay(5000);
	beep();
}

// Interrupt on pin 2 to accept shutdown request
void shutDownISR() {
	shutDownRequestReceived = true;
	Serial.println("Responder ISR");
}

void beep() {
	pinMode(11, OUTPUT);
	digitalWrite(11, HIGH);
	delay(50);
	digitalWrite(11, LOW);
}
