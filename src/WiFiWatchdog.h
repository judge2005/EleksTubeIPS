#ifndef WIFIWATCHDOG_H_
#define WIFIWATCHDOG_H_

/*
 * Reconnects Wi-Fi when it can't send. A burst of web traffic while memory is short can leave the
 * Wi-Fi driver refusing everything it's given to send, for good, though it still receives and stays
 * connected. The clock then looks frozen: no ping, web UI, weather or MQTT. Reconnecting clears it.
 */
namespace WiFiWatchdog {
	void loop();	// Every so often, from one task
}

#endif /* WIFIWATCHDOG_H_ */
