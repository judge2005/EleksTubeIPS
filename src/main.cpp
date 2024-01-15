#include <Arduino.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <AsyncWiFiManager.h>
#include <EspSNTPTimeSync.h>
#include <EspRTCTimeSync.h>
#include <ConfigItem.h>
#include <EEPROMConfig.h>

#include "TFTs.h"
#include "IPSClock.h"
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
IPSClock ipsClock;

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
BooleanConfigItem dimming("dimming", false);

BaseConfigItem *clockSet[] = {
	// Clock
	&IPSClock::getTimeOrDate(),
	&IPSClock::getDateFormat(),
	&IPSClock::getHourFormat(),
	&IPSClock::getLeadingZero(),
	&IPSClock::getDisplayOn(),
	&IPSClock::getDisplayOff(),
	&IPSClock::getClockFace(),
	&IPSClock::getTimeZone(),
	&dimming,
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
	&IPSClock::getClockFace(),
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

void clockTaskFn(void *pArg) {
	ipsClock.init();
	ipsClock.setTimeSync(timeSync);

	while (true) {
		delay(1);

		if (ipsClock.clockOn()) {
			ipsClock.setDimming(false);
		} else {
			ipsClock.setDimming(dimming);
		}
		ipsClock.loop();
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
			if (strcmp(key, IPSClock::getTimeZone().name) == 0) {
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
		if (ipsClock.clockOn()) {
			backlights.setOn(true);
			backlights.setDimming(false);
		} else {
			if (dimming) {
				backlights.setOn(true);
				backlights.setDimming(true);
			} else {
				backlights.setOn(false);
				backlights.setDimming(false);
			}
		}
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

	createSSID();

	EEPROM.begin(2048);
	initFromEEPROM();

	timeSync = new EspSNTPTimeSync(IPSClock::getTimeZone().value, asyncTimeSetCallback, asyncTimeErrorCallback);

	rtcTimeSync = new EspRTCTimeSync(SDApin, SCLpin);
	rtcTimeSync->init();

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

	/*
	 * Very occasionally the wifi stack will close with a beacon timeout error.
	 *
	 * This is apparently related to it not being able to allocate a buffer at
	 * some point because of a transitory memory issue.
	 *
	 * In older versions this issue is not well handled so the wifi stack never
	 * recovers. The work-around is to not allow the wifi radio to sleep.
	 *
	 * https://github.com/espressif/esp-idf/issues/11615
	 */
	esp_wifi_set_ps(WIFI_PS_NONE);

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
