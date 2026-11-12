/*
 * Limits how much of the heap the Wi-Fi driver can take for frames it has received.
 *
 * The driver copies each frame it receives into a buffer taken from the heap, and keeps it until lwIP
 * has dealt with it. Arduino lets it keep up to 32, and a frame from a router can need 2 KB or more, so a
 * burst can hold 50-65 KB of the clock's 60-70 KB, and then everything else that needs memory (the web
 * server, lwIP, MQTT, the weather) can fail to get it. Bursts come after a scan for networks (opening the
 * access point does one), when a phone joins the access point, and under heavy traffic.
 *
 * Measured on a V1, flooding it with UDP: with 32 the driver held 53-66 KB and free heap fell to 2-3 KB,
 * with hundreds of failed allocations. With 8 it held 16-19 KB, free heap stayed above 36 KB and nothing
 * failed. Speeds through the router didn't change, as lwIP's receive window (5744 bytes, 4 frames) limits
 * them first, and uploads over the access point were about 10% slower. Heavy web traffic gains less, as
 * sending takes most of the memory then: the driver's peak went from 30-47 KB to 27-32 KB. Espressif's
 * lowest-memory settings also use 8.
 *
 * Arduino sets this number itself in WiFi.mode(), so it's changed on its way to esp_wifi_init(), which
 * the linker sends here (-Wl,--wrap=esp_wifi_init in platformio.ini).
 */
#include <esp_wifi.h>

namespace {
	const int maxReceiveBuffers = 8;
}

extern "C" {
	esp_err_t __real_esp_wifi_init(const wifi_init_config_t *config);

	esp_err_t __wrap_esp_wifi_init(const wifi_init_config_t *config) {
		wifi_init_config_t limited = *config;

		// 0 means no limit
		if (limited.dynamic_rx_buf_num == 0 || limited.dynamic_rx_buf_num > maxReceiveBuffers) {
			limited.dynamic_rx_buf_num = maxReceiveBuffers;
		}

		return __real_esp_wifi_init(&limited);
	}
}
