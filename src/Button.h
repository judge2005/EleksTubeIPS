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
	enum Event {none, button_clicked, long_press};

	Button() {
	}

	bool state();
	bool clicked();
	Event getEvent();
	void setCallback(std::function<void(const Button*, Event)> callback);

protected:
	virtual byte getPinValue() = 0;
	void dispatchEvent(Event);

	unsigned long closedTime = 0;
	unsigned long clickTime = 0;
	bool wasPressed = false;
	bool wasLongPress = false;

	std::function<void(const Button*, Event)> callback;
};

#endif /* LIBRARIES_NIXIEMISC_BUTTON_H_ */
