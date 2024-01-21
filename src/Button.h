/*
 * Button.h
 *
 *  Created on: Apr 15, 2018
 *      Author: mpand
 */

#ifndef LIBRARIES_NIXIEMISC_BUTTON_H_
#define LIBRARIES_NIXIEMISC_BUTTON_H_

#include <Arduino.h>

class Button {
public:
	Button() {
	}

	bool state();
	bool clicked();

protected:
	virtual byte getPinValue() = 0;

	int lastButtonValue = 0;
	unsigned long closedTime = 0;
	unsigned long clickTime = 0;
	bool wasPressed = false;
};

#endif /* LIBRARIES_NIXIEMISC_BUTTON_H_ */
