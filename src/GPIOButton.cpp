/*
 * GPIOButton.cpp
 *
 *  Created on: May 10, 2019
 *      Author: mpand
 */

#include "GPIOButton.h"

byte GPIOButton::getPinValue() {
	return digitalRead(pin) == pulldown;
}
