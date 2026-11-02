#include "WiFiWatchdog.h"

#include <WiFi.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>

namespace {
	// Under load a few sends fail now and then, and the next ones go
	const unsigned long stuckMs = 15000;
	const uint32_t stuckFailures = 10;

	netif_linkoutput_fn send = NULL;	// The station interface's own
	bool hooked = false;

	// Since the last frame that went. Written on lwIP's thread.
	volatile unsigned long failingSince = 0;
	volatile uint32_t failures = 0;

	err_t sendCounted(struct netif *netif, struct pbuf *p) {
		err_t err = send(netif, p);

		if (err == ERR_OK) {
			failures = 0;
		} else if (failures++ == 0) {
			failingSince = millis();
		}

		return err;
	}

	// On lwIP's thread, which is the only one that sends
	void hook(void *arg) {
		struct netif *netif = (struct netif *)arg;

		send = netif->linkoutput;
		netif->linkoutput = sendCounted;
	}
}

void WiFiWatchdog::loop() {
	if (!hooked) {
		esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");	// Once Wi-Fi has started
		if (station != NULL) {
			hooked = tcpip_callback(hook, esp_netif_get_netif_impl(station)) == ERR_OK;
		}
		return;
	}

	if (failures >= stuckFailures && millis() - failingSince > stuckMs) {
		failures = 0;
		Serial.println("Wi-Fi couldn't send for 15 seconds, reconnecting");
		WiFi.reconnect();
	}
}
