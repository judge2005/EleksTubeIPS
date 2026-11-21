#ifndef SETTINGS_BACKUP_H
#define SETTINGS_BACKUP_H

#include <Arduino.h>

class AsyncWebServer;

// The Configuration page's backup and restore of the settings as a JSON file, see SettingsBackup.cpp
namespace SettingsBackup {
	// Answers /settings.json
	void begin(AsyncWebServer &server);

	// The file sets, any of "faces", "weather" and "slides", whose choice a restore changed. They were only
	// saved, so the clock unpacks them when it starts. Forgets them once asked.
	String takeChangedFileSets();
}

#endif
