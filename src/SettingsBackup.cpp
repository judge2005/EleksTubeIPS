/*
 * Backs up the settings to a JSON file and restores them from one, for the web UI's Configuration page.
 *
 * GET /settings.json downloads every setting, grouped as they're stored, with the passwords and the API key
 * left empty, and the name of the Wi-Fi network but not its password. POST /settings.json checks a file and
 * says what restoring it would change. POST /settings.json?apply restores it: it saves the settings, then
 * restarts the clock, which starts with them. That way nothing acts on half of them, such as the MQTT client
 * connecting again for each of its four settings, and a clock face that changed is unpacked as it starts.
 *
 * Settings are found by name, whatever group they're in, so a file from another version of the firmware or
 * another clock restores whatever this clock has. Ones the file doesn't have stay as they are, and so do
 * passwords left empty, so someone can type theirs into the file, or on the page, and restore those too.
 * Each value gets the checks the web UI or Home Assistant would give it.
 */
#include <Arduino.h>
#include <algorithm>
#include <vector>
#include <time.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ConfigItem.h>
#include <EEPROMConfig.h>

#include "SettingsBackup.h"
#include "IRAMPtrArray.h"
#include "IPSClock.h"
#include "ImageUnpacker.h"
#include "mqttBroker.h"

extern IRAMPtrArray<const char*> manifest;
extern CompositeConfigItem rootConfig;
extern EEPROMConfig config;
extern String chipId;
extern StringConfigItem hostName;
extern StringConfigItem webUser;
extern SemaphoreHandle_t wsMutex;
extern SemaphoreHandle_t memMutex;
extern const char *passwordPlaceholder;
bool validHostname(const String &name);
bool validLogin(const StringConfigItem &item, const String &value);

// So a file can be recognised, and told apart from one a later version writes differently
static const char *const fileType = "EleksTubeIPS settings";
static const int fileFormat = 1;

// Never in a backup. A restore only changes them if the file has them: empty, or the web UI's placeholder,
// leaves them as they are.
static const char *const secrets[] = { "web_password", "mqtt_password", "weather_token", 0 };

// Settings that could lock someone out of the clock, or make it clash with the clock a file came from. The
// Configuration page asks about each group, see configuration.js.
struct CarefulSetting {
	const char *item;
	const char *group;
};

static const CarefulSetting carefulSettings[] = {
	{ "hostname", "hostname" },
	{ "static_ip", "address" },
	{ "static_gateway", "address" },
	{ "static_subnet", "address" },
	{ "static_dns1", "address" },
	{ "static_dns2", "address" },
	{ "web_user", "login" },
	{ "web_password", "login" }
};

// Numbers the web UI limits that MQTTBroker::checkSetting() doesn't know, as Home Assistant can't change them.
// Other byte settings can be anything a byte holds.
struct Range {
	const char *item;
	long min;
	long max;
};

static const Range ranges[] = {
	{ "time_or_date", IPSClock::TIME, IPSClock::SLIDE_SHOW },
	{ "brightness_config", 10, 255 },
	{ "screen_saver", 0, 1 },
	{ "mqtt_port", 1, 65535 }
};

// The settings that choose a file, and the directory under /ips it has to be in
struct FileSetting {
	const char *item;
	const char *dir;
};

static const FileSetting fileSettings[] = {
	{ "clock_face", "faces" },
	{ "weather_icons", "weather" },
	{ "slide_show", "slides" }
};

// The file sets a restore changed, one per line, see takeChangedFileSets()
static const char *const changedFileSetsFile = "/ips/restored";

// The biggest file a restore takes. A backup is about 2 KB.
static const size_t maxFileSize = 8192;

// What a restore changes. They're all checked before any is saved.
struct Change {
	BaseConfigItem *item;
	String value;
};

// What restartTaskFn() saves
static bool restarting = false;
static std::vector<Change> pendingChanges;
static char newSSID[33];
static char newWiFiPassword[65];

enum ItemKind { SWITCH_ITEM, NUMBER_ITEM, TEXT_ITEM };

// A config item doesn't say what it holds, but what it sends the web UI shows it
static ItemKind kindOf(const BaseConfigItem *item) {
	String json = item->toJSON();
	if (json.startsWith("\"")) {
		return TEXT_ITEM;
	}

	return json == "true" || json == "false" ? SWITCH_ITEM : NUMBER_ITEM;
}

// Its value in the form a checked value from a file is in
static String currentValue(const BaseConfigItem *item, ItemKind kind) {
	return kind == TEXT_ITEM ? ((const StringConfigItem*)item)->value : item->toJSON();
}

// As a JSON string, number or boolean
static void putValue(JsonVariant dest, ItemKind kind, const String &value) {
	if (kind == TEXT_ITEM) {
		dest.set(value);
	} else if (kind == SWITCH_ITEM) {
		dest.set(value == "true");
	} else {
		dest.set(value.toInt());
	}
}

// A group, like "web_login", rather than a setting
static bool isGroup(const BaseConfigItem *item) {
	if (item == &rootConfig) {
		return true;
	}

	for (int i = 0; rootConfig.value[i] != 0; i++) {
		if (item == rootConfig.value[i]) {
			return true;
		}
	}

	return false;
}

static bool isSecret(const char *name) {
	for (int i = 0; secrets[i] != 0; i++) {
		if (strcmp(name, secrets[i]) == 0) {
			return true;
		}
	}

	return false;
}

static const char *carefulGroup(const char *name) {
	for (const CarefulSetting &setting : carefulSettings) {
		if (strcmp(name, setting.item) == 0) {
			return setting.group;
		}
	}

	return 0;
}

static const FileSetting *fileSetting(const char *name) {
	for (const FileSetting &setting : fileSettings) {
		if (strcmp(name, setting.item) == 0) {
			return &setting;
		}
	}

	return 0;
}

static bool contains(const std::vector<const BaseConfigItem*> &items, const BaseConfigItem *item) {
	return std::find(items.begin(), items.end(), item) != items.end();
}

// The Wi-Fi network the clock joins when it starts, and its password
static void savedWiFi(String &ssid, String &password) {
	wifi_config_t conf;
	if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) {
		return;
	}

	// Neither has to end with a 0 if it fills the whole field
	char text[sizeof(conf.sta.password) + 1];
	memcpy(text, conf.sta.ssid, sizeof(conf.sta.ssid));
	text[sizeof(conf.sta.ssid)] = 0;
	ssid = text;
	memcpy(text, conf.sta.password, sizeof(conf.sta.password));
	text[sizeof(conf.sta.password)] = 0;
	password = text;
}

// What WPA2 takes: 8 to 63 characters, or 64 hexadecimal digits
static bool validWiFiPassword(const String &password) {
	if (password.length() == 64) {
		for (int i = 0; i < 64; i++) {
			if (!isxdigit((unsigned char)password[i])) {
				return false;
			}
		}
		return true;
	}

	for (int i = 0; i < password.length(); i++) {
		if (password[i] < ' ' || password[i] > '~') {
			return false;
		}
	}

	return password.length() >= 8 && password.length() <= 63;
}

static String exportSettings() {
	JsonDocument doc;
	doc["file"] = fileType;
	doc["format"] = fileFormat;
	doc["firmware"] = manifest[1];
	doc["clock"] = manifest[0];
	doc["clock_id"] = chipId;

	JsonObject settings = doc["settings"].to<JsonObject>();
	std::vector<const BaseConfigItem*> done;	// The clock face is in two groups
	for (int g = 0; rootConfig.value[g] != 0; g++) {
		const CompositeConfigItem *group = (const CompositeConfigItem*)rootConfig.value[g];
		JsonObject values = settings[group->name].to<JsonObject>();
		for (int i = 0; group->value[i] != 0; i++) {
			const BaseConfigItem *item = group->value[i];
			if (contains(done, item)) {
				continue;
			}
			done.push_back(item);

			ItemKind kind = kindOf(item);
			putValue(values[item->name].to<JsonVariant>(), kind, isSecret(item->name) ? String() : currentValue(item, kind));
		}
	}

	String ssid, password;
	savedWiFi(ssid, password);
	JsonObject wifi = doc["wifi"].to<JsonObject>();
	wifi["ssid"] = ssid;
	wifi["password"] = "";

	String json;
	serializeJsonPretty(doc, json);	// For people to read and edit
	return json;
}

// What the web UI would send: true or false, a whole number or text. False for anything else, like an object.
static bool asString(JsonVariantConst value, String &text) {
	if (value.is<bool>()) {
		text = value.as<bool>() ? "true" : "false";
	} else if (value.is<long>()) {
		text = String(value.as<long>());
	} else if (value.is<double>()) {
		serializeJson(value, text);	// As written, e.g. a latitude. A whole-number setting refuses it.
	} else if (value.is<const char*>()) {
		text = value.as<const char*>();
	} else {
		return false;
	}

	return true;
}

enum Verdict { RESTORE, REFUSE, SKIP };

// Whether a restore can give item this value, and why not. Puts the value in the form the clock keeps it in.
static Verdict checkValue(BaseConfigItem *item, ItemKind kind, String &value, String &why) {
	const char *name = item->name;

	if (kind == SWITCH_ITEM) {
		why = "Must be true or false";
		return value == "true" || value == "false" ? RESTORE : REFUSE;
	}

	if (kind == NUMBER_ITEM) {
		char *end;
		long number = strtol(value.c_str(), &end, 10);
		if (value.length() == 0 || *end != 0) {
			why = "Must be a whole number";
			return REFUSE;
		}
		value = String(number);

		long min = item->maxSize == 1 ? 0 : INT32_MIN;
		long max = item->maxSize == 1 ? 255 : INT32_MAX;
		for (const Range &range : ranges) {
			if (strcmp(name, range.item) == 0) {
				min = range.min;
				max = range.max;
			}
		}
		if (number < min || number > max) {
			why = String("Must be a whole number from ") + min + " to " + max;
			return REFUSE;
		}

		return MQTTBroker::checkSetting(name, value, why) ? RESTORE : REFUSE;
	}

	// Text. A stored setting stops at the first character that isn't ASCII, see validLogin() in main.cpp.
	if (value.length() > item->maxSize) {
		why = String("At most ") + item->maxSize + " characters";
		return REFUSE;
	}
	for (int i = 0; i < value.length(); i++) {
		if (value[i] < ' ' || value[i] > '~') {
			why = "Only letters, digits, spaces and punctuation, without accents";
			return REFUSE;
		}
	}

	const char *group = carefulGroup(name);
	const FileSetting *file = fileSetting(name);
	if (item == &hostName) {
		why = "Only letters, digits and hyphens, without a hyphen at either end";
		return validHostname(value) ? RESTORE : REFUSE;
	} else if (item == &webUser) {
		why = "Only letters, digits and . _ - @";
		return validLogin(webUser, value) ? RESTORE : REFUSE;
	} else if (group != 0 && strcmp(group, "address") == 0) {
		// Like the Wi-Fi portal's Configure Static WiFi page, see setAddress() in main.cpp
		IPAddress address;
		if (value.length() > 0 && (!address.fromString(value) || (uint32_t)address == 0)) {
			why = "Must be an IP address, like 192.168.1.50, or empty";
			return REFUSE;
		}
		value = value.length() > 0 ? address.toString() : String();
	} else if (strcmp(name, "file_set") == 0) {
		why = "Must be faces, weather or slides";
		return value == "faces" || value == "weather" || value == "slides" ? RESTORE : REFUSE;
	} else if (file != 0) {
		if (!ImageUnpacker::isValidName(value)) {
			why = "Not a file name";
			return REFUSE;
		}
		if (!LittleFS.exists(String("/ips/") + file->dir + "/" + value + ".tar.gz")) {
			why = "Not on this clock, so it stays as it is";
			return SKIP;
		}
	} else if (!MQTTBroker::checkSetting(name, value, why)) {
		return REFUSE;
	}

	return RESTORE;
}

// As a String, which the report copies. It keeps a const char* as a pointer, and names from the file go with it.
static void addReason(JsonArray list, const String &name, const String &why) {
	JsonObject entry = list.add<JsonObject>();
	entry["name"] = name;
	entry["why"] = why;
}

// The value a setting will have after the restore
static String resultingValue(const char *name, const std::vector<Change> &changes) {
	for (const Change &change : changes) {
		if (strcmp(change.item->name, name) == 0) {
			return change.value;
		}
	}

	BaseConfigItem *item = rootConfig.get(name);
	return item != 0 ? currentValue(item, kindOf(item)) : String();
}

// Saves the restored settings and restarts
static void restartTaskFn(void *arg) {
	delay(1000);	// Let the reply go first

	// Kept until the restart, so the clock task can't act on the new settings before then, such as starting to
	// unpack a clock face the restart would cut short. Anything that holds it, like an unpack, finishes first.
	xSemaphoreTake(memMutex, portMAX_DELAY);

	String changedSets;
	for (const Change &change : pendingChanges) {
		change.item->fromString(change.value);
		change.item->put();

		const FileSetting *fileSet = fileSetting(change.item->name);
		if (fileSet != 0) {
			changedSets += String(fileSet->dir) + "\n";
		}
	}
	config.commit();

	if (changedSets.length() > 0) {
		fs::File sets = LittleFS.open(changedFileSetsFile, "a");
		if (sets) {
			sets.print(changedSets);
			sets.close();
		}
	}

	if (newSSID[0] != 0) {
		// Only saved, for the clock to join as it starts. The Wi-Fi driver keeps it, as the Wi-Fi manager
		// made Wi-Fi persistent.
		xSemaphoreTake(wsMutex, portMAX_DELAY);	// Held while the Wi-Fi manager runs
		WiFi.begin(newSSID, newWiFiPassword, 0, NULL, false);
		xSemaphoreGive(wsMutex);
	}

	ESP.restart();
}

// Checks a settings file against the clock, and if apply, restores it. Says what it found, or would do, in report.
static void restore(const JsonDocument &file, bool apply, bool onAP, JsonDocument &report) {
	JsonObjectConst settings = file["settings"];
	if (settings.isNull()) {
		report["error"] = "That isn't a settings file from the clock. It has no settings in it.";
		return;
	}

	bool sameClock = chipId == (file["clock_id"] | "");
	report["same_clock"] = sameClock;
	report["on_ap"] = onAP;

	JsonArray warnings = report["warnings"].to<JsonArray>();
	String firmware = file["firmware"] | "";
	if (firmware.length() == 0) {
		warnings.add(String("It doesn't say which firmware saved it. This clock has ") + manifest[1] + ".");
	} else if (firmware != manifest[1]) {
		warnings.add("It was saved by firmware " + firmware + ", and this clock has " + manifest[1] + ".");
	}
	if ((file["format"] | 0) > fileFormat) {
		warnings.add("It was saved by newer firmware, which may have saved some settings in a way this clock doesn't understand.");
	}
	String clock = file["clock"] | "";
	if (clock.length() > 0 && clock != manifest[0]) {
		warnings.add("It's from a different kind of clock: " + clock + ".");
	}
	if (!sameClock) {
		warnings.add("It's from another clock, so it only restores the settings that should differ between clocks if you switch them on below.");
	}

	JsonArray changes = report["changes"].to<JsonArray>();
	JsonArray refused = report["refused"].to<JsonArray>();
	JsonArray skipped = report["skipped"].to<JsonArray>();
	int unchanged = 0;
	std::vector<Change> toChange;
	std::vector<const BaseConfigItem*> seen;

	for (JsonPairConst group : settings) {
		JsonObjectConst values = group.value().as<JsonObjectConst>();
		if (values.isNull()) {
			addReason(skipped, group.key().c_str(), "Not a group of settings");
			continue;
		}

		for (JsonPairConst pair : values) {
			const char *name = pair.key().c_str();
			BaseConfigItem *item = rootConfig.get(name);
			if (item == 0 || isGroup(item)) {
				addReason(skipped, name, "Not a setting on this clock");
				continue;
			}
			if (contains(seen, item)) {
				continue;	// Only the first counts
			}
			seen.push_back(item);

			String value;
			if (!asString(pair.value(), value)) {
				addReason(refused, name, "Must be text, a number, or true or false");
				continue;
			}

			bool secret = isSecret(name);
			if (secret && (value.length() == 0 || value == passwordPlaceholder)) {
				continue;	// Stays as it is
			}

			ItemKind kind = kindOf(item);
			String why;
			Verdict verdict = checkValue(item, kind, value, why);
			if (verdict != RESTORE) {
				addReason(verdict == REFUSE ? refused : skipped, name, why);
				continue;
			}

			// A password or key from the file is always restored, never compared with the clock's, so the page
			// can't be used to find out whether a guess is right
			if (!secret && value == currentValue(item, kind)) {
				unchanged++;
				continue;
			}

			JsonObject change = changes.add<JsonObject>();
			change["name"] = String(name);	// Copied, see addReason()
			if (secret) {
				change["secret"] = true;	// Its value isn't sent back
			} else {
				putValue(change["from"].to<JsonVariant>(), kind, currentValue(item, kind));
				putValue(change["to"].to<JsonVariant>(), kind, value);
			}
			const char *careful = carefulGroup(name);
			if (careful != 0) {
				change["group"] = careful;
			}
			toChange.push_back({ item, value });
		}
	}

	// The clock's settings the file doesn't have
	JsonArray kept = report["kept"].to<JsonArray>();
	std::vector<const BaseConfigItem*> listed;
	for (int g = 0; rootConfig.value[g] != 0; g++) {
		const CompositeConfigItem *group = (const CompositeConfigItem*)rootConfig.value[g];
		for (int i = 0; group->value[i] != 0; i++) {
			const BaseConfigItem *item = group->value[i];
			if (!isSecret(item->name) && !contains(seen, item) && !contains(listed, item)) {
				kept.add(item->name);
				listed.push_back(item);
			}
		}
	}

	// Secrets the file left empty, which the page can ask for: those it has are all in toChange. Not a password
	// for a login or broker that won't be used.
	JsonArray blank = report["blank"].to<JsonArray>();
	for (int i = 0; secrets[i] != 0; i++) {
		const char *name = secrets[i];
		bool changing = false;
		for (const Change &change : toChange) {
			changing = changing || strcmp(change.item->name, name) == 0;
		}
		if (changing || rootConfig.get(name) == 0) {
			continue;
		}
		if (strcmp(name, "web_password") == 0 && resultingValue("web_user", toChange).length() == 0) {
			continue;
		}
		if (strcmp(name, "mqtt_password") == 0 && resultingValue("mqtt_host", toChange).length() == 0) {
			continue;
		}
		blank.add(name);
	}

	// The network the clock joins. Only over its access point, as changing the network that the page reaches it
	// through, wrongly, would leave it unreachable.
	bool wifiChange = false;
	JsonObjectConst wifi = file["wifi"];
	String ssid = wifi["ssid"] | "";
	String password = wifi["password"] | "";
	if (!wifi.isNull()) {
		report["wifi_ssid"] = ssid;
		if (password.length() == 0 || password == passwordPlaceholder) {
			if (onAP && ssid.length() > 0) {
				blank.add("wifi");
			}
		} else if (!onAP) {
			// The page reached the clock through the network it's on, so that's left alone. Its name isn't secret.
			String current = WiFi.SSID();
			if (current.length() == 0) {
				addReason(skipped, "wifi", "Only changed over the clock's access point");
			} else if (ssid == current) {
				addReason(skipped, "wifi", "The clock is already on " + current + ". Wi-Fi is only changed over the clock's access point.");
			} else {
				addReason(skipped, "wifi", "Only changed over the clock's access point, so the clock stays on " + current + ".");
			}
		} else if (ssid.length() == 0 || ssid.length() > 32) {
			addReason(refused, "wifi", "The network's name must be 1 to 32 characters");
		} else if (!validWiFiPassword(password)) {
			addReason(refused, "wifi", "The password must be 8 to 63 letters, digits, spaces and punctuation, or 64 hexadecimal digits");
		} else {
			// Like a password or key, not compared with what the clock has
			JsonObject change = changes.add<JsonObject>();
			change["name"] = "wifi";
			change["to"] = ssid;
			change["secret"] = true;
			change["group"] = "wifi";
			wifiChange = true;
		}
	}

	report["unchanged"] = unchanged;

	if (!apply || (toChange.empty() && !wifiChange)) {
		return;
	}

	pendingChanges = toChange;
	newSSID[0] = 0;
	if (wifiChange) {
		strlcpy(newSSID, ssid.c_str(), sizeof(newSSID));
		strlcpy(newWiFiPassword, password.c_str(), sizeof(newWiFiPassword));
	}

	if (xTaskCreate(restartTaskFn, "Restore restart", 4096, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		report["error"] = "The clock is short of memory, so nothing was restored. Try again in a minute.";
		return;
	}
	restarting = true;
	report["restart"] = true;
}

static void handleGet(AsyncWebServerRequest *request) {
	// A file name from the hostname, which a setting saved before names were checked could have anything in
	String name = hostName.value;
	for (int i = 0; i < name.length(); i++) {
		if (!isalnum((unsigned char)name[i]) && name[i] != '-') {
			name[i] = '_';
		}
	}
	name += "-settings";

	// Then the date and time by the clock, so a browser doesn't save the next one as "...-settings(1).json".
	// Left out until the clock knows the time, as it starts in 1970.
	time_t now = time(NULL);
	struct tm local;
	if (localtime_r(&now, &local) != NULL && local.tm_year >= 2025 - 1900) {
		char when[64];	// snprintf rather than strftime, which would make the firmware 4 KB bigger
		snprintf(when, sizeof(when), "-%04d-%02d-%02d-%02d%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
			local.tm_hour, local.tm_min);
		name += when;
	}

	AsyncWebServerResponse *response = request->beginResponse(200, "application/json", exportSettings());
	response->addHeader("Content-Disposition", "attachment; filename=\"" + name + ".json\"");
	response->addHeader("Cache-Control", "no-store");
	request->send(response);
}

// The file arrives in pieces. They're kept with the request, which frees them.
static void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
	if (index == 0 && total <= maxFileSize) {
		request->_tempObject = calloc(total + 1, 1);
	}

	if (request->_tempObject != NULL && index + len <= total) {
		memcpy((uint8_t*)request->_tempObject + index, data, len);
	}
}

static void handlePost(AsyncWebServerRequest *request) {
	JsonDocument report;

	if (request->_tempObject == NULL) {
		report["error"] = request->contentLength() > maxFileSize ? "That file is too big to be a settings file." : "No file was sent.";
	} else if (restarting) {
		report["error"] = "The clock is restarting.";
	} else {
		JsonDocument file;
		DeserializationError error = deserializeJson(file, (const char*)request->_tempObject);
		if (error) {
			report["error"] = String("That isn't a settings file. It isn't JSON: ") + error.c_str();
		} else {
			restore(file, request->hasParam("apply"), ON_AP_FILTER(request), report);
		}
	}

	String json;
	serializeJson(report, json);
	request->send(200, "application/json", json);
}

void SettingsBackup::begin(AsyncWebServer &server) {
	server.on("/settings.json", HTTP_GET, handleGet);
	server.on("/settings.json", HTTP_POST, handlePost, NULL, handleBody);
}

String SettingsBackup::takeChangedFileSets() {
	String sets;
	if (LittleFS.exists(changedFileSetsFile)) {
		fs::File file = LittleFS.open(changedFileSetsFile, "r");
		if (file) {
			sets = file.readString();
			file.close();
		}
		LittleFS.remove(changedFileSetsFile);
	}

	return sets;
}
