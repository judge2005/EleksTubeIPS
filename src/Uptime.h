/*
 * Uptime.h
 *
 *  Created on: Nov 27, 2019
 *      Author: mpand
 */

#ifndef LIBRARIES_NIXIEMISC_UPTIME_H_
#define LIBRARIES_NIXIEMISC_UPTIME_H_

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif

class Uptime {
public:
	Uptime() {
#ifdef ESP32
		utMutex = xSemaphoreCreateMutex();
#endif
	}
	char *uptime();
	void loop();

private:
#ifdef ESP32
	SemaphoreHandle_t utMutex;
#endif
	unsigned long long rollover = 0;
	unsigned long lastMillis;
	char _return[32];
};

#endif /* LIBRARIES_NIXIEMISC_UPTIME_H_ */
