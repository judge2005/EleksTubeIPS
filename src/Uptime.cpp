/*
 * Uptime.cpp
 *
 *  Created on: Nov 27, 2019
 *      Author: mpand
 */

#include <Uptime.h>
#include <Arduino.h>

void Uptime::loop() {
#ifdef ESP32
	if (xSemaphoreTake(utMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
		return;
	}
#endif

	unsigned long now = millis();
	if (lastMillis > now) {
		rollover++;
	}
	lastMillis = now;

#ifdef ESP32
	xSemaphoreGive(utMutex);
#endif
}

char *Uptime::uptime() {
#ifdef ESP32
	if (xSemaphoreTake(utMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
		*_return = 0;
		return _return;
	}
#endif

	unsigned long long _now = (rollover << 32) + lastMillis;
	unsigned long secs = _now / 1000LL, mins = secs / 60;
	unsigned long hours = mins / 60, days = hours / 24;
	secs -= mins * 60;
	mins -= hours * 60;
	hours -= days * 24;
	sprintf(_return, "%d days %02dh %02dm %02ds", (int) days,
			(int) hours, (int) mins, (int) secs);

#ifdef ESP32
	xSemaphoreGive(utMutex);
#endif
		
	return _return;
}
    
