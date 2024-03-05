#include <IPAddress.h>
#include <WiFi.h>
#include <AsyncWiFiManager.h>
#include <ArduinoJson.h>
#include <ConfigItem.h>

#include "ScreenSaver.h"
#include "mqttBroker.h"
#include "IRAMPtrArray.h"

extern AsyncWiFiManager *wifiManager;
extern CompositeConfigItem rootConfig;
extern ScreenSaver screenSaver;
extern void broadcastUpdate(const BaseConfigItem&);
extern IRAMPtrArray<char*> manifest;
extern byte brightness;
extern SemaphoreHandle_t memMutex;

void MQTTBroker::onConnect(bool sessionPresent)
{
	reconnect = false;
	Serial.println("Connected to MQTT");
    sendHADiscoveryMessage();
}

void MQTTBroker::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
	Serial.printf("Disconnected from MQTT: %u\n", static_cast<uint8_t>(reason));

	if (WiFi.isConnected())
	{
		reconnect = true;
		lastReconnect = millis();
	}
}

void MQTTBroker::onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length)
{
	uint8_t mqttMessageBuffer[10];

		// payload is bigger then max: return chunked
	if (total_length >= sizeof(mqttMessageBuffer)) {
		DEBUG("MQTT message too large");
		return;
	}

	// add data and dispatch when done
	memcpy(&mqttMessageBuffer[index], payload, length);
	if (index + length == total_length) {
		// message is complete here
		mqttMessageBuffer[total_length] = 0;

        if (strcmp(topic, screenSaverTopic) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) {
                screenSaver.reset();
            } else {
                screenSaver.start();
            }
            publishState();
        } else if (strcmp(topic, screenSaverDelayTopic) == 0) {
            ScreenSaver::getScreenSaverDelay() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(ScreenSaver::getScreenSaverDelay());
        } else if (strcmp(topic, brightnessTopic) == 0) {
            brightness = atoi((const char*)mqttMessageBuffer);
            publishState();
        } else {
            Serial.print("Unknown topic: ");
            Serial.print(topic);
            Serial.print(", value: ");
            Serial.println((char*)mqttMessageBuffer);
        }
	}
}

bool MQTTBroker::init(const String& id) {
    this->id = id;

    client.disconnect();

    IPAddress ipAddress;
    if (ipAddress.fromString(getHost().value.c_str())) {
        sprintf(volatileStateTopic, "clock/%s/volatile/state", id.c_str());
        sprintf(persistentStateTopic, "clock/%s/persistent/state", id.c_str());
        sprintf(screenSaverTopic, "clock/%s/screen_saver/set", id.c_str());
        sprintf(brightnessTopic, "clock/%s/brightness/set", id.c_str());
        sprintf(screenSaverDelayTopic, "clock/%s/screen_saver_delay/set", id.c_str());

        client.setServer(getHost().value.c_str(), getPort());
        client.setCredentials(getUser().value.c_str(),getPassword().value.c_str());
        client.onConnect([this](bool sessionPresent) { this->onConnect(sessionPresent); });
        client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) { this->onDisconnect(reason); });
        client.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length) {
            this->onMessage(topic, payload, properties, length, index, total_length); });
        
        screenSaver.setChangeCallback([this](bool isOff) { this->publishState(); });
        
        reconnect = true;
    }

    return reconnect;
}

void MQTTBroker::connect() {
    if (WiFi.isConnected() && !wifiManager->isAP()) {
        Serial.println("Connecting to MQTT...");
        client.connect();
    }
}

void MQTTBroker::checkConnection() {
    if (reconnect && (millis() - lastReconnect >= 2000)) {
        connect();
    }
}

void MQTTBroker::publishState() {
    if (client.connected()) {
        // Takes a lot of memory
        if (xSemaphoreTake(memMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            JsonDocument volatileState;
            volatileState["screen_saver_on"] = screenSaver.isOn() ? "ON" : "OFF";
            volatileState["brightness"] = brightness;
            char buffer[256];
            size_t n = serializeJson(volatileState, buffer);

            client.publish(volatileStateTopic, 0, false, buffer);
            client.publish(persistentStateTopic, 0, false, rootConfig.toJSON().c_str());
            xSemaphoreGive(memMutex);
        } else {
            // Serial.println("Gave up waiting for memory semaphore");
        }
    }
}

void MQTTBroker::sendHADiscoveryMessage() {
    client.subscribe(screenSaverTopic, 0);
    client.subscribe(screenSaverDelayTopic, 0);
    client.subscribe(brightnessTopic, 0);

    // This is the discovery topic for the 'screensaver' switch
    char discoveryTopic[128];
    sprintf(discoveryTopic, "homeassistant/switch/%s/screen_saver/config", id.c_str());

    JsonDocument doc;
    char buffer[512];

    doc["name"] = "Screen Saver";
    doc["icon"] = "mdi:sleep";
    doc["unique_id"] = "screen_saver_" + id;
    doc["stat_t"] = volatileStateTopic;
    doc["cmd_t"] = screenSaverTopic;
    doc["val_tpl"] = "{{ value_json.screen_saver_on }}";
    doc["device"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["device"]["name"] = manifest[3];
    doc["device"]["identifiers"][0] = WiFi.macAddress();
    doc["device"]["model"] = manifest[0];
    doc["device"]["sw_version"] = manifest[1];

    size_t n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 0, true, buffer, n);

    sprintf(discoveryTopic, "homeassistant/number/%s/screen_saver_delay/config", id.c_str());

    doc.clear();

    doc["name"] = "Screen Saver Delay";
    doc["icon"] = "mdi:knob";
    doc["unique_id"] = "screen_saver_delay" + id;
    doc["stat_t"] = persistentStateTopic;
    doc["cmd_t"] = screenSaverDelayTopic;
    doc["min"] = 0;
    doc["max"] = 120;
    doc["val_tpl"] = "{{ value_json.matrix.screen_saver_delay }}";
    doc["device"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["device"]["name"] = manifest[3];
    doc["device"]["identifiers"][0] = WiFi.macAddress();
    doc["device"]["model"] = manifest[0];
    doc["device"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 0, true, buffer, n);

    sprintf(discoveryTopic, "homeassistant/number/%s/brightness/config", id.c_str());

    doc.clear();

    doc["name"] = "Brightness";
    doc["icon"] = "mdi:brightness-6";
    doc["unique_id"] = "brightness" + id;
    doc["stat_t"] = volatileStateTopic;
    doc["cmd_t"] = brightnessTopic;
    doc["min"] = 10;
    doc["max"] = 255;
    doc["val_tpl"] = "{{ value_json.brightness }}";
    doc["device"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["device"]["name"] = manifest[3];
    doc["device"]["identifiers"][0] = WiFi.macAddress();
    doc["device"]["model"] = manifest[0];
    doc["device"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 0, true, buffer, n);

    publishState();
}