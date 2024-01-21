/*
 * Button.cpp
 *
 *  Created on: Apr 15, 2018
 *      Author: mpand
 */

#include "Button.h"

bool Button::state() {
	int buttonValue = getPinValue();	// Closed == 1, Open == 0

	unsigned long now = millis();

	if (!(now - closedTime > 50)) {
		// Still in blackout period, we will return true for at least 50ms
		// after the initial close in case the switch is bouncy.
		return true;
	} else if (buttonValue) {
		// Outside blackout period, and the button is down.
		// Reset closedTime - so we will report closed for at least 50ms after the last close
		closedTime = now;

		return true;
	} else {
		// Outside blackout period and the button is up, so it is safe to say it REALLY is up.
		return false;
	}
}

bool Button::clicked() {
	// A click means that the button needs to be released within say 0.5s of being pushed,
	// otherwise it is a press. Problem with this algorithm is that this method needs
	// to be called within 0.5s after the button down state is recorded. Also, of the button
	// is pressed and we don't call this method for, say 10s, we will only start the timer
	// when we get called.

	bool nowPressed = state();

	unsigned long now = millis();

	if (!wasPressed) {
		// Button wasn't down last time we were called
		if (nowPressed) {
			// Button went down, start timer
			clickTime = now;
			wasPressed = true;
			return false;
		} else {
			// Button isn't down now either
			wasPressed = false;
			return false;
		}
	} else {
		// Button was down last time we were called
		if (!nowPressed) {
			// Button went up, was it a click or not?
			wasPressed = false;
			if (now - clickTime > 500) {
				// Too long
				return false;
			} else {
				// Yes it was!
				return true;
			}
		} else {
			// Button is still down, won't know if it was a click
			// until it goes up
			return false;
		}
	}
}

