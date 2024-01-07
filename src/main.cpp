#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <AsyncWiFiManager.h>
#include <EspSNTPTimeSync.h>
#include <EspRTCTimeSync.h>
#include <ConfigItem.h>
#include <EEPROMConfig.h>
#include <ESP32-targz.h>

#include "TFTs.h"
#include "Backlights.h"
#include "WSHandler.h"
#include "WSMenuHandler.h"
#include "WSConfigHandler.h"
#include "WSInfoHandler.h"

#define DEBUG(...) { Serial.println(__VA_ARGS__); }
#ifndef DEBUG
#define DEBUG(...) {  }
#endif

String getChipId(void);
void setWiFiAP(bool on);
void infoCallback();
String clockFacesCallback();

TFTs tfts;
Backlights backlights;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
DNSServer dns;
AsyncWiFiManager wifiManager(&server,&dns);
TimeSync *timeSync;
RTCTimeSync *rtcTimeSync;

TaskHandle_t wifiManagerTask;
TaskHandle_t clockTask;
TaskHandle_t ledTask;
TaskHandle_t testTask;
TaskHandle_t commitEEPROMTask;

SemaphoreHandle_t wsMutex;

AsyncWiFiManagerParameter *hostnameParam;
String ssid("EleksTubeIPS");
String chipId = getChipId();

#define SCLpin (22)
#define SDApin (21)

// Persistent Configuration
StringConfigItem hostName("hostname", 63, "elekstubeips");

// Clock config
BooleanConfigItem time_or_date("time_or_date", true);	// time
ByteConfigItem date_format("date_format", 1);			// mm-dd-yy, dd-mm-yy, yy-mm-dd
BooleanConfigItem hour_format("hour_format", true);	// 12/24 hour
BooleanConfigItem leading_zero("leading_zero", false);	//
ByteConfigItem display_on("display_on", 6);
ByteConfigItem display_off("display_off", 24);
StringConfigItem clock_face("clock_face", 25, "divergence");	// <clock_face>.tar.gz, max length is 31
StringConfigItem time_zone("time_zone", 63, "EST5EDT,M3.2.0,M11.1.0");	// POSIX timezone format
StringConfigItem dummy("dummy", 2, "!");	

BaseConfigItem *clockSet[] = {
	// Clock
	&time_or_date,
	&date_format,
	&hour_format,
	&leading_zero,
	&display_on,
	&display_off,
	&clock_face,
	&time_zone,
	&dummy,
	0
};
CompositeConfigItem clockConfig("clock", 0, clockSet);

BaseConfigItem *ledSet[] = {
    // LEDs
    &Backlights::getLEDPattern(),
    &Backlights::getColorPhase(),
    &Backlights::getLEDIntensity(),
    &Backlights::getBreathPerMin(),
    0
};
CompositeConfigItem ledConfig("leds", 0, ledSet);

BaseConfigItem *faceSet[] = {
	// Faces
	&clock_face,
	0
};
CompositeConfigItem facesConfig("faces", 0, faceSet);

// Extra config items
ByteConfigItem mov_delay("mov_delay", 20);
ByteConfigItem mov_src("mov_src", 1);

BaseConfigItem *extraSet[] = {
	// Extra
	&mov_delay,
	&mov_src,
	0
};
CompositeConfigItem extraConfig("extra", 0, extraSet);

// Sync config values
IntConfigItem sync_port("sync_port", 4920);
ByteConfigItem sync_role("sync_role", 0);	// 0 = none, 1 = master, 2 = slave
BaseConfigItem *syncSet[] = {
	// Sync
	&sync_port,
	&sync_role,
	0
};
CompositeConfigItem syncConfig("sync", 0, syncSet);

// Global configuration
BaseConfigItem *configSetGlobal[] = {
	&hostName,
	0
};

CompositeConfigItem globalConfig("global", 0, configSetGlobal);

// All configurations
BaseConfigItem *configSetRoot[] = {
	&globalConfig,
	&clockConfig,
	&ledConfig,
	&facesConfig,
	&extraConfig,
	&syncConfig,
	0
};

CompositeConfigItem rootConfig("root", 0, configSetRoot);

// Store the configurations in EEPROM
EEPROMConfig config(rootConfig);

namespace ClockTimer {

class Timer {
public:
	Timer(unsigned long duration) : enabled(false), lastTick(0), duration(duration) {}
	Timer() : enabled(false), lastTick(0), duration(0) {}

	bool expired(unsigned long now) const {
		return now - lastTick >= duration;
	}
	void setDuration(unsigned long duration) {
		this->duration = duration;
	}
	void init(unsigned long now, unsigned long duration) {
		lastTick = now;
		this->duration = duration;
	}
	void reset(unsigned long duration) {
		lastTick += this->duration;
		this->duration = duration;
	}
	unsigned long getLastTick() const {
		return lastTick;
	}
	unsigned long getDuration() const {
		return duration;
	}

	bool isEnabled() const {
		return enabled;
	}

	void setEnabled(bool enabled) {
		this->enabled = enabled;
	}

private:
	bool enabled;
	unsigned long lastTick;
	unsigned long duration;
};

} /* namespace ClockTimer */

void asyncTimeSetCallback(String time) {
	DEBUG(time);
	tfts.setStatus("NTP time received...");

	rtcTimeSync->enabled(false);
	rtcTimeSync->setDS3231();
}

void asyncTimeErrorCallback(String msg) {
	DEBUG(msg);
	rtcTimeSync->enabled(true);
}

bool clockOn() {
	struct tm now;
	suseconds_t uSec;
	timeSync->getLocalTime(&now, &uSec);

	if (display_on.value == display_off.value) {
		return true;
	}

	if (display_on.value < display_off.value) {
		return now.tm_hour >= display_on.value && now.tm_hour < display_off.value;
	}

	if (display_on.value > display_off.value) {
		return !(now.tm_hour >= display_off.value && now.tm_hour < display_on.value);
	}

	return false;
}

bool newUnpack = true;
const char* unpackName = "";
int currentFaceNum = 0;
int numberOfFaces = 10;

void statusProgressCallback(const char* name, size_t size, size_t total_unpacked) {
	unpackName = name;
	currentFaceNum++;
}

void unpackProgressCallback(uint8_t progress) {
	uint8_t totalProgress = currentFaceNum * numberOfFaces + progress / numberOfFaces;
	if (totalProgress > 100) {
		totalProgress = 100;
	}
	tfts.drawMeter(totalProgress, newUnpack, unpackName);
	newUnpack = false;
}

void cacheClockFace(const String &faceName) {
	// mount spiffs (or any other filesystem)
    static TarGzUnpacker *TARGZUnpacker = new TarGzUnpacker();

    TARGZUnpacker->setTarVerify( true ); // true = enables health checks but slows down the overall process
    TARGZUnpacker->setupFSCallbacks( targzTotalBytesFn, targzFreeBytesFn ); // prevent the partition from exploding, recommended
    TARGZUnpacker->setGzProgressCallback(BaseUnpacker::defaultProgressCallback); // targzNullProgressCallback or defaultProgressCallback
    TARGZUnpacker->setLoggerCallback( BaseUnpacker::targzPrintLoggerCallback  );    // gz log verbosity
    TARGZUnpacker->setTarProgressCallback( unpackProgressCallback ); // prints the untarring progress for each individual file
    TARGZUnpacker->setTarStatusProgressCallback( statusProgressCallback ); // print the filenames as they're expanded
    TARGZUnpacker->setTarMessageCallback( BaseUnpacker::targzPrintLoggerCallback ); // tar log verbosity

	String fileName("/ips/faces/" + faceName + ".tar.gz");

	currentFaceNum = -1;
	newUnpack = true;

    if( !TARGZUnpacker->tarGzExpander(tarGzFS, fileName.c_str(), tarGzFS, "/ips/cache", nullptr ) ) {
    	Serial.printf("tarGzExpander+intermediate file failed with return code #%d\n", TARGZUnpacker->tarGzGetError() );
    } else {
		Serial.println("File unzipped");
	}

	fs::File dir = LittleFS.open("/ips/cache");
    File file = dir.openNextFile();
    while(file){
		Serial.print("  FILE: ");
		Serial.print(file.name());
		Serial.print("  SIZE: ");

#ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
		Serial.println(file.size());
#else
		Serial.print(file.size());
		time_t t= file.getLastWrite();
		struct tm * tmstruct = localtime(&t);
		Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
#endif
		file.close();
        file = dir.openNextFile();
    }
}

void clockTaskFn(void *pArg) {
	static ClockTimer::Timer displayTimer;
	static String oldClockFace;

	displayTimer.setEnabled(true);

	while (true) {
		delay(1);

		if (clock_face.value != oldClockFace) {
			oldClockFace = clock_face.value;
			cacheClockFace(clock_face.value);
			tfts.claim();
			tfts.invalidateAllDigits();
			tfts.release();
		}
		
		unsigned long nowMs = millis();

		if (displayTimer.expired(nowMs)) {
			struct tm now;
			suseconds_t uSec;
			timeSync->getLocalTime(&now, &uSec);
			suseconds_t realms = uSec / 1000;
			if (realms > 1000) {
				realms = realms % 1000;	// Something went wrong so pick a safe number for 1000 - realms...
			}
			unsigned long tDelay = 1000 - realms;

			if (clockOn()) {
				tfts.claim();
				tfts.checkStatus();
				tfts.enableAllDisplays();
				if (time_or_date.value) {
					uint8_t hour = now.tm_hour;
					if (hour_format.value) {
						if (now.tm_hour > 12) {
							hour = now.tm_hour - 12;
						} else if (now.tm_hour == 0) {
							hour = 12;
						}
					}

					// refresh starting on seconds
					tfts.setDigit(SECONDS_ONES, now.tm_sec % 10, TFTs::yes);
					tfts.setDigit(SECONDS_TENS, now.tm_sec / 10, TFTs::yes);
					tfts.setDigit(MINUTES_ONES, now.tm_min % 10, TFTs::yes);
					tfts.setDigit(MINUTES_TENS, now.tm_min / 10, TFTs::yes);
					tfts.setDigit(HOURS_ONES, hour % 10, TFTs::yes);
					if (hour < 10 && !leading_zero.value) {
						tfts.setDigit(HOURS_TENS, TFTs::blanked, TFTs::yes);
					} else {
						tfts.setDigit(HOURS_TENS, hour / 10, TFTs::yes);
					}
				} else {
					uint8_t day = now.tm_mday;
					uint8_t month = now.tm_mon;
					uint8_t year = now.tm_year;

					switch (date_format.value) {
					case 0:	// DD-MM-YY
						break;
					case 1: // MM-DD-YY
						day = now.tm_mon;
						month = now.tm_mday;
						break;
					default: // YY-MM-DD
						day = now.tm_year;
						year = now.tm_mday;
						break;
					}

					// refresh starting on 'seconds'
					tfts.setDigit(SECONDS_ONES, year % 10, TFTs::yes);
					tfts.setDigit(SECONDS_TENS, year / 10, TFTs::yes);
					tfts.setDigit(MINUTES_ONES, month % 10, TFTs::yes);
					tfts.setDigit(MINUTES_TENS, month / 10, TFTs::yes);
					tfts.setDigit(HOURS_ONES, day % 10, TFTs::yes);
					tfts.setDigit(HOURS_TENS, day / 10, TFTs::yes);
				}
			} else {
				tfts.disableAllDisplays();
			}

			tfts.release();

			displayTimer.init(nowMs, tDelay);
		}
	}
}

String getChipId(void)
{
  uint8_t macid[6];
  esp_efuse_mac_get_default(macid);
  String chipId = String((uint32_t)(macid[5] + (((uint32_t)(macid[4])) << 8) + (((uint32_t)(macid[3])) << 16)), HEX);
  chipId.toUpperCase();
  return chipId;
}

void createSSID() {
	// Create a unique SSID that includes the hostname. Max SSID length is 32!
	ssid = (chipId + hostName).substring(0, 31);
}

String *items[] = {
	&WSMenuHandler::clockMenu,
	&WSMenuHandler::ledsMenu,
	&WSMenuHandler::facesMenu,
	// &WSMenuHandler::syncMenu,
	// &WSMenuHandler::presetsMenu,
	&WSMenuHandler::infoMenu,
	// &WSMenuHandler::presetNamesMenu,
	0
};

WSMenuHandler wsMenuHandler(items);
WSConfigHandler wsClockHandler(rootConfig, "clock");
WSConfigHandler wsLEDHandler(rootConfig, "leds");
WSConfigHandler wsFacesHandler(rootConfig, "faces", clockFacesCallback);
WSInfoHandler wsInfoHandler(infoCallback);

// Order of this needs to match the numbers in WSMenuHandler.cpp
WSHandler *wsHandlers[] = {
	&wsMenuHandler,
	&wsClockHandler,
	&wsLEDHandler,
	&wsFacesHandler,
	// &wsPresetValuesHandler,
	NULL,
	&wsInfoHandler,
	// &wsPresetNamesHandler,
	NULL,
	// &wsSyncHandler,
	NULL
};

void infoCallback() {
	wsInfoHandler.setSsid(ssid);
	// wsInfoHandler.setBlankingMonitor(&blankingMonitor);
	// wsInfoHandler.setRevision(revision);

	wsInfoHandler.setFSSize(String(LittleFS.totalBytes()));
	wsInfoHandler.setFSFree(String(LittleFS.totalBytes() - LittleFS.usedBytes()));
	TimeSync::SyncStats &syncStats = timeSync->getStats();

	wsInfoHandler.setFailedCount(syncStats.failedCount);
	wsInfoHandler.setLastFailedMessage(syncStats.lastFailedMessage);
	wsInfoHandler.setLastUpdateTime(syncStats.lastUpdateTime);

	// wsInfoHandler.setClockOn(nixieDriver.getDisplayOn() ? "on" : "off");
	// wsInfoHandler.setBrightness(String(ldr.getNormalizedBrightness(true)));
	// if (lastMoved == 0) {
	// 	wsInfoHandler.setTriggered("never");	// Probably
	// } else {
	// 	unsigned long diff = (millis() - lastMoved) / 1000;
	// 	String value;
	// 	value = value + (diff / 3600) + "h " + ((diff / 60) % 60) + "m " + (diff % 60) + "s ago";
	// 	wsInfoHandler.setTriggered(value);
	// }
}

void broadcastUpdate(const BaseConfigItem& item) {
	xSemaphoreTake(wsMutex, portMAX_DELAY);

	const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(20);	// 20 should be plenty
	DynamicJsonDocument doc(bufferSize);
	JsonObject root = doc.to<JsonObject>();

	root["type"] = "sv.update";

	JsonVariant value = root.createNestedObject("value");
	String rawJSON = item.toJSON();	// This object needs to hang around until we are done serializing.
	value[item.name] = serialized(rawJSON.c_str());

    size_t len = measureJson(root);
    AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
    if (buffer) {
    	serializeJson(root, (char *)buffer->get(), len + 1);
    	ws.textAll(buffer);
    }

	xSemaphoreGive(wsMutex);
}

void updateValue(int screen, String pair) {
	int index = pair.indexOf(':');
	DEBUG(pair)
	// _key has to hang around because key points to an internal data structure
	String _key = pair.substring(0, index);
	const char* key = _key.c_str();
	String value = pair.substring(index+1);
	if (screen == 6) {
		BaseConfigItem *item = rootConfig.get(key);
		if (item != 0) {
			item->fromString(value);
			item->put();
			broadcastUpdate(*item);
		}
	} else {
		BaseConfigItem *item = rootConfig.get(key);
		if (item != 0) {
			item->fromString(value);
			item->put();
			// Shouldn't special case this stuff. Should attach listeners to the config value!
			// TODO: This won't work if we just switch change sets instead!
			if (strcmp(key, time_zone.name) == 0) {
				timeSync->setTz(value);
				timeSync->sync();
			}
			broadcastUpdate(*item);
		} else if (_key == "sync_do") {
			// sendSyncMsg();
			// announceSlave();
		} else if (_key == "wifi_ap") {
			setWiFiAP(value == "true" ? true : false);
		} else if (_key == "hostname") {
			hostName = value;
			hostName.put();
			config.commit();
			ESP.restart();
		}
	}
}

/*
 * Handle application protocol
 */
void handleWSMsg(AsyncWebSocketClient *client, char *data) {
	String wholeMsg(data);
	int code = wholeMsg.substring(0, wholeMsg.indexOf(':')).toInt();

	if (code < 9) {
		if (code < sizeof(wsHandlers)/sizeof(wsHandlers[0])) {
			if (wsHandlers[code] != NULL) {
				wsHandlers[code]->handle(client, data);
			}
		}
	} else {
		String message = wholeMsg.substring(wholeMsg.indexOf(':')+1);
		int screen = message.substring(0, message.indexOf(':')).toInt();
		String pair = message.substring(message.indexOf(':')+1);
		updateValue(screen, pair);
	}
}

void wsHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
	//Handle WebSocket event
	switch (type) {
	case WS_EVT_CONNECT:
		DEBUG("WS connected")
		;
		break;
	case WS_EVT_DISCONNECT:
		DEBUG("WS disconnected")
		;
		break;
	case WS_EVT_ERROR:
		DEBUG("WS error")
		;
		DEBUG((char* )data)
		;
		break;
	case WS_EVT_PONG:
		DEBUG("WS pong")
		;
		break;
	case WS_EVT_DATA:	// Yay we got something!
		DEBUG("WS data")
		;
		AwsFrameInfo * info = (AwsFrameInfo*) arg;
		if (info->final && info->index == 0 && info->len == len) {
			//the whole message is in a single frame and we got all of it's data
			if (info->opcode == WS_TEXT) {
				DEBUG("WS text data");
				data[len] = 0;
				handleWSMsg(client, (char *) data);
			} else {
				DEBUG("WS binary data");
			}
		} else {
			DEBUG("WS data was split up!");
		}
		break;
	}
}

void mainHandler(AsyncWebServerRequest *request) {
	DEBUG("Got request")
	request->send(LittleFS, "/index.html");
}

void timeHandler(AsyncWebServerRequest *request) {
	DEBUG("Got time request")
	String wifiTime = request->getParam("time", true, false)->value();

	DEBUG(String("Setting time from wifi manager") + wifiTime);

	timeSync->setTime(wifiTime);
	rtcTimeSync->setTime(wifiTime);

	request->send(LittleFS, "/time.html");
}

void sendFavicon(AsyncWebServerRequest *request) {
	DEBUG("Got favicon request")
	request->send(LittleFS, "/assets/favicon-32x32.png", "image/png");
}

void handleDelete(AsyncWebServerRequest *request) {
	DEBUG("Got delete request");

	String filename = request->pathArg(0);

	DEBUG(filename);

	if (filename.length() > 0) {
		if (LittleFS.remove("/ips/faces/" + filename)) {
			request->send(200, "text/plain", "File deleted");

			wsFacesHandler.broadcast(ws, 0);
			return;
		}
	}

	request->send(500, "text/plain", "Delete dailed");
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
	if (!index)
	{
		DEBUG((String) "UploadStart: " + filename);
		// open the file on first call and store the file handle in the request object
		request->_tempFile = LittleFS.open("/ips/faces/" + filename, "wb", true);
	}
	if (len)
	{
		// stream the incoming chunk to the opened file
		DEBUG((String) "Writing: " + len + " byted");
		request->_tempFile.write(data, len);
	}
	if (final)
	{
		DEBUG((String) "UploadEnd: " + filename);
		// close the file handle as the upload is now done
		request->_tempFile.close();
		request->send(200, "text/plain", "File uploaded");

		wsFacesHandler.broadcast(ws, 0);
	}
}

void configureWebServer() {
	server.serveStatic("/", LittleFS, "/");
	server.on("/", HTTP_GET, mainHandler).setFilter(ON_STA_FILTER);
	server.on("/t", HTTP_POST, timeHandler).setFilter(ON_AP_FILTER);
	server.on("/assets/favicon-32x32.png", HTTP_GET, sendFavicon);
	server.on("/upload_face", HTTP_POST, [](AsyncWebServerRequest *request) {
    	request->send(200);
    }, handleUpload);
	server.on("^\\/delete_face\\/(.*\\.tar\\.gz)$", HTTP_DELETE, handleDelete);
	server.serveStatic("/assets", LittleFS, "/assets");
	
#ifdef OTA
	otaUpdater.init(server, "/update", sendUpdateForm, sendUpdatingInfo);
#endif

	// attach AsyncWebSocket
	ws.onEvent(wsHandler);
	server.addHandler(&ws);
	server.begin();
	ws.enable(true);
}

String clockFacesCallback() {
	static const String postfix(".tar.gz");
	static const String quote("\"");
	static const String quoteColonQuote("\":\"");
	static const String comma_quote(",\"");

	String options = ",\"face_files\":{";
	String sep = quote;
	fs::File dir = LittleFS.open("/ips/faces");
    String name = dir.getNextFileName();
    while(name.length() > 0){
		String fileName = name.substring(11, name.length());
		String option = fileName.substring(0, fileName.lastIndexOf(postfix));
		options += sep;
		options += option;
		options += quoteColonQuote;
		options += fileName;
		options += quote;
		sep = comma_quote;
        name = dir.getNextFileName();
    }
	dir.close();

	options += "}";

	return options;
}

void ledTaskFn(void *pArg) {
	backlights.begin();

	while (true) {
		backlights.setOn(clockOn());
		backlights.loop();
		delay(16);
	}
}

void commitEEPROMTaskFn(void *pArg) {
	while(true) {
		delay(60000);
//		DEBUG("Committing config");
		config.commit();
	}
}

void initFromEEPROM() {
//	config.setDebugPrint(debugPrint);
	config.init();
//	rootConfig.debug(debugPrint);
	rootConfig.get();	// Read all of the config values from EEPROM

	hostnameParam = new AsyncWiFiManagerParameter("Hostname", "clock host name", hostName.value.c_str(), 63);
}

void connectedHandler() {
	tfts.setStatus(WiFi.localIP().toString());
	DEBUG("connectedHandler");
	MDNS.end();
	MDNS.begin(hostName.value.c_str());
	MDNS.addService("http", "tcp", 80);
}

void apChange(AsyncWiFiManager *wifiManager) {
	DEBUG("apChange()");
	DEBUG(wifiManager->isAP());
	if (wifiManager->isAP()) {
		tfts.setStatus(ssid);
	} else {
		tfts.setStatus("AP Destroyed...");
	}
}

void setWiFiAP(bool on) {
	if (on) {
		wifiManager.startConfigPortal(ssid.c_str(), "secretsauce");
	} else {
		wifiManager.stopConfigPortal();
	}
}

void SetupServer() {
	DEBUG("SetupServer()");
	hostName = String(hostnameParam->getValue());
	hostName.put();
	config.commit();
	createSSID();
	wifiManager.setAPCredentials(ssid.c_str(), "secretsauce");
	DEBUG(hostName.value.c_str());
	MDNS.begin(hostName.value.c_str());
	MDNS.addService("http", "tcp", 80);
//	getTime();
}

void wifiManagerTaskFn(void *pArg) {
	while(true) {
		xSemaphoreTake(wsMutex, portMAX_DELAY);
		wifiManager.loop();
		xSemaphoreGive(wsMutex);

		delay(50);
	}
}

void setup() {
	/*
	* setup() runs on core 1
	*/
	Serial.begin(115200);
	Serial.setDebugOutput(true);

	DEBUG("Setup...");

	wsMutex = xSemaphoreCreateMutex();

	LittleFS.begin();

	// Setup TFTs
	tfts.begin(LittleFS, "/ips/cache");
	tfts.fillScreen(TFT_BLACK);
	tfts.setTextColor(TFT_WHITE, TFT_BLACK);
	tfts.setCursor(0, 0, 2);
	tfts.setStatus("setup...");

	timeSync = new EspSNTPTimeSync(time_zone.value, asyncTimeSetCallback, asyncTimeErrorCallback);

	rtcTimeSync = new EspRTCTimeSync(SDApin, SCLpin);
	rtcTimeSync->init();

	createSSID();

	EEPROM.begin(2048);
	initFromEEPROM();

    xTaskCreatePinnedToCore(
          commitEEPROMTaskFn, /* Function to implement the task */
          "Commit EEPROM task", /* Name of the task */
		  4096,  /* Stack size in words */
          NULL,  /* Task input parameter */
		  tskIDLE_PRIORITY,  /* More than background tasks */
          &commitEEPROMTask,  /* Task handle. */
		  xPortGetCoreID()
		  );

    xTaskCreatePinnedToCore(
		ledTaskFn, /* Function to implement the task */
		"led task", /* Name of the task */
		4096,  /* Stack size in words */
		NULL,  /* Task input parameter */
		tskIDLE_PRIORITY + 2,  /* Priority of the task (idle) */
		&ledTask,  /* Task handle. */
		1	/*  */
	);

	xTaskCreatePinnedToCore(
		clockTaskFn, /* Function to implement the task */
		"Clock task", /* Name of the task */
		4096,  /* Stack size in words */
		NULL,  /* Task input parameter */
		tskIDLE_PRIORITY + 1,  /* More than background tasks */
		&clockTask,  /* Task handle. */
		0
	);

	tfts.setStatus("Connecting...");

	wifiManager.setDebugOutput(true);
	wifiManager.setHostname(hostName.value.c_str());	// name router associates DNS entry with
	wifiManager.setCustomOptionsHTML("<br><form action='/t' name='time_form' method='post'><button name='time' onClick=\"{var now=new Date();this.value=now.getFullYear()+','+(now.getMonth()+1)+','+now.getDate()+','+now.getHours()+','+now.getMinutes()+','+now.getSeconds();} return true;\">Set Clock Time</button></form><br><form action=\"/app.html\" method=\"get\"><button>Configure Clock</button></form>");
	wifiManager.addParameter(hostnameParam);
	wifiManager.setSaveConfigCallback(SetupServer);
	wifiManager.setConnectedCallback(connectedHandler);
	wifiManager.setConnectTimeout(2000);	// milliseconds
	wifiManager.setAPCallback(apChange);
	wifiManager.setAPCredentials(ssid.c_str(), "secretsauce");
	wifiManager.start();

	configureWebServer();

	xTaskCreatePinnedToCore(
		wifiManagerTaskFn,    /* Function to implement the task */
		"WiFi Manager task",  /* Name of the task */
		4096,                 /* Stack size in words */
		NULL,                 /* Task input parameter */
		tskIDLE_PRIORITY + 2, /* Priority of the task (idle) */
		&wifiManagerTask,     /* Task handle. */
		0
	);

	timeSync->init();

    Serial.print("setup() running on core ");
    Serial.println(xPortGetCoreID());
    vTaskDelete(NULL);	// Delete this task (so loop() won't be called)
}

void loop() {
}
