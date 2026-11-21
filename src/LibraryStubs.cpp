/*
 * Stand-ins for functions in Espressif's precompiled libraries whose results the clock never uses, so the
 * real ones, and what they bring in, are left out of the firmware. The linker sends calls here
 * (-Wl,--wrap=<function> in platformio.ini).
 *
 * Measured on a V1: about 35 KB smaller, and 5.7 KB more free memory from leaving out core dumps.
 */
#include <stdio.h>
#include <esp_err.h>
#include <esp_core_dump.h>

extern "C" {
	/*
	 * mbedTLS's texts for its error codes, 15 KB. WiFiClientSecure only makes one to log it with log_e(),
	 * which CORE_DEBUG_LEVEL 0 leaves out. This writes the code instead, such as -0x7780.
	 */
	void __wrap_mbedtls_strerror(int errnum, char *buffer, size_t buflen) {
		if (buflen > 0) {
			snprintf(buffer, buflen, "-0x%04X", (unsigned) (errnum < 0 ? -errnum : errnum));
		}
	}

	/*
	 * ESP-IDF's names for its error codes, 7.4 KB, which only appear in its messages on the serial port. They
	 * show the number instead, such as 0x105 for ESP_ERR_NOT_FOUND.
	 */
	const char *__wrap_esp_err_to_name(esp_err_t code) {
		// The longest, "0xffffffff", leaves the last byte 0, so the text always ends, even if another task
		// is writing it at the same time
		static char name[12];
		snprintf(name, sizeof(name) - 1, "0x%x", (unsigned) code);
		return name;
	}

	const char *__wrap_esp_err_to_name_r(esp_err_t code, char *buf, size_t buflen) {
		if (buflen > 0) {
			snprintf(buf, buflen, "0x%x", (unsigned) code);
		}
		return buf;
	}

	/*
	 * Writing a core dump to the coredump partition when the clock crashes, 12 KB, and 5.7 KB of memory
	 * while it runs. Nothing reads one back, and the crash and backtrace are still printed on the serial port.
	 */
	void __wrap_esp_core_dump_init(void) {
	}

	void __wrap_esp_core_dump_to_flash(panic_info_t *) {
	}
}
