#include <IPAddress.h>
#include <WiFi.h>
#include <AsyncWiFiManager.h>
#include <ArduinoJson.h>
#include <ConfigItem.h>

#include "ScreenSaver.h"
#include "Backlights.h"
#include "mqttBroker.h"
#include "IPSClock.h"

extern AsyncWiFiManager *wifiManager;
extern CompositeConfigItem rootConfig;
extern ScreenSaver *screenSaver;
extern void broadcastUpdate(const BaseConfigItem&);
extern IRAMPtrArray<const char*> manifest;
extern SemaphoreHandle_t memMutex;
extern QueueHandle_t mainQueue;

static const char *device_s = "dev";
IRAMPtrArray<const char*> MQTTBroker::displayStates {
    "Time",
    "Date",
    "Weather",
    "Slideshow",
    0
};

void MQTTBroker::onConnect(bool sessionPresent)
{
	reconnect = false;
//	Serial.println("Connected to MQTT");
    client.subscribe("homeassistant/status", 2);
    sendHADiscoveryMessage();
}

#ifdef ASYNC_MQTT_HA_CLIENT
void MQTTBroker::onDisconnect(AsyncMqttClientDisconnectReason reason)
#else
void MQTTBroker::onDisconnect(espMqttClientTypes::DisconnectReason reason)
#endif
{
//	Serial.printf("Disconnected from MQTT: %u\n", static_cast<uint8_t>(reason));

    reconnect = true;
    lastReconnect = millis();
}

#ifdef ASYNC_MQTT_HA_CLIENT
void MQTTBroker::onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length)
#else
void MQTTBroker::onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length)
#endif
{
	uint8_t mqttMessageBuffer[32];

		// payload is bigger then max: return chunked
	if (total_length >= sizeof(mqttMessageBuffer)) {
		Serial.printf("MQTT message too large (%d): topic=%s, payload=%s\n", total_length, topic, payload);
		return;
	}

	// add data and dispatch when done
	memcpy(&mqttMessageBuffer[index], payload, length);
	if (index + length == total_length) {
		// message is complete here
		mqttMessageBuffer[total_length] = 0;

        /*
         e.g. home = "/fred"
         topic = "/fred/topic"
         topicIndex = 5
         */
        int topicIndex = strlen(home);
        if (strcmp(topic, "homeassistant/status") == 0) {
            if (strcmp("online", (const char*)mqttMessageBuffer)) {
                Serial.println("HA online");
                sendHADiscoveryMessage();
            }
        } else if (strcmp(topic + topicIndex, screenSaverTopic + 1) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) { 
                screenSaver->reset();
            } else {
                screenSaver->start();
            }
        } else if (strcmp(topic + topicIndex, screenSaverDelayTopic + 1) == 0) {
            ScreenSaver::getScreenSaverDelay() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(ScreenSaver::getScreenSaverDelay());
        } else if (strcmp(topic + topicIndex, brightnessTopic + 1) == 0) {
            IPSClock::getBrightnessConfig() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(IPSClock::getBrightnessConfig());
        } else if (strcmp(topic + topicIndex, displayTopic + 1) == 0) {
            IPSClock::getTimeOrDate() = atoi((const char*)mqttMessageBuffer);
            broadcastUpdate(IPSClock::getTimeOrDate());
            IPSClock::getTimeOrDate().notify();
        } else if (strcmp(topic + topicIndex, customDataTopic + 1) == 0) {
            // TODO: get the max data from somewhere
            if (length <= 6){
                // empty string or clear/CLEAR will clear the custom data
                if (strcmp((const char*)mqttMessageBuffer, "CLEAR") == 0
                    || strcmp((const char*)mqttMessageBuffer, "clear") == 0){
                    IPSClock::getCustomData() = "";
                }
                else{
                    IPSClock::getCustomData() = (const char*)mqttMessageBuffer;
                }
            }
            else{
//                Serial.println("Custom data too long, ignoring");
            }
        } else if (strcmp(topic + topicIndex, backlightStateTopic + 1) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) {
                Backlights::backlightState = false;
            } else {
                Backlights::backlightState = true;
            }
        } else if (strcmp(topic + topicIndex, backlightHSTopic + 1) == 0) {
            float hue, sat;
            int count;

            // The format string "%f,%f" tells sscanf to read a float, then a comma, then another float.
            count = sscanf((const char*)mqttMessageBuffer, "%f,%f", &hue, &sat);

            if (count == 2) {
                Backlights::backlightHue = hue * 255 / 360;
                Backlights::backlightSaturation = sat * 255 / 100;
            }
        } else if (strcmp(topic + topicIndex, backlightBrightnessTopic + 1) == 0) {
            Backlights::backlightBrightness = atoi((const char*)mqttMessageBuffer);
#if (NUM_LEDS > 6)
        } else if (strcmp(topic + topicIndex, underlightStateTopic + 1) == 0) {
            if (strcmp((const char*)mqttMessageBuffer, "OFF") == 0) {
                Backlights::backlightState = false;
            } else {
                Backlights::backlightState = true;
            }
        } else if (strcmp(topic + topicIndex, underlightHSTopic + 1) == 0) {
            float hue, sat;
            int count;

            // The format string "%f,%f" tells sscanf to read a float, then a comma, then another float.
            count = sscanf((const char*)mqttMessageBuffer, "%f,%f", &hue, &sat);

            if (count == 2) {
                Backlights::underlightHue = hue * 255 / 360;
                Backlights::underlightSaturation = sat * 255 / 100;
            }
        } else if (strcmp(topic + topicIndex, underlightBrightnessTopic + 1) == 0) {
            Backlights::underlightBrightness = atoi((const char*)mqttMessageBuffer);
#endif
        } else {
            // Serial.print("Unknown topic: ");
            // Serial.print(topic);
            // Serial.print(", value: ");
            // Serial.println((char*)mqttMessageBuffer);
        }
	}

    uint32_t msg = 1;
	xQueueSend(mainQueue, &msg, pdMS_TO_TICKS(100));
}

bool MQTTBroker::init(const String& id) {
    this->id = id;

    client.disconnect();

    IPAddress ipAddress;
    if (ipAddress.fromString(getHost().value.c_str())) {
        sprintf(home, "clock/%s", id.c_str());
        sprintf(volatileStateTopic, "clock/%s/volatile/state", id.c_str());
        sprintf(persistentStateTopic, "clock/%s/persistent/state", id.c_str());
        sprintf(availabilityTopic, "clock/%s/availability", id.c_str());

        client.setServer(getHost().value.c_str(), getPort());
        client.setCredentials(getUser().value.c_str(),getPassword().value.c_str());
        client.setWill(availabilityTopic, 2, true, "offline");
        client.onConnect([this](bool sessionPresent) { this->onConnect(sessionPresent); });
#ifdef ASYNC_MQTT_HA_CLIENT
        client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) { this->onDisconnect(reason); });
        client.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length) {
            this->onMessage(topic, payload, properties, length, index, total_length); });
#else
        client.onDisconnect([this](espMqttClientTypes::DisconnectReason reason) { this->onDisconnect(reason); });
        client.onMessage([this](const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length) {
            this->onMessage(properties, topic, payload, length, index, total_length); });
#endif
        screenSaver->setChangeCallback([this](bool isOff) { 
            uint32_t msg = 1;
            xQueueSend(mainQueue, &msg, pdMS_TO_TICKS(100));
        });
        
        reconnect = true;
    }

    return reconnect;
}

void MQTTBroker::connect() {
    if (WiFi.isConnected() && !wifiManager->isAP()) {
//        Serial.println("Connecting to MQTT...");
        client.connect();
    }
}

void MQTTBroker::checkConnection() {
    if (reconnect && (millis() - lastReconnect >= 2000)) {
        lastReconnect = millis();
        connect();
    }
}

void MQTTBroker::publishState() {
    if (client.connected()) {
        // Takes a lot of memory
        if (xSemaphoreTake(memMutex, pdMS_TO_TICKS(500)) == pdTRUE)
        {
            JsonDocument volatileState;
            volatileState["screen_saver_on"] = screenSaver->isOn() ? "ON" : "OFF";
            volatileState["brightness"] = IPSClock::getBrightnessConfig().value;
            volatileState["custom"] = IPSClock::getCustomData().value;
            volatileState["display"] = IPSClock::getTimeOrDate().value;

            volatileState["backlight_state"] = Backlights::backlightState ? "ON" : "OFF";
            JsonArray blArray = volatileState["backlight_hs"].to<JsonArray>();
            blArray.add(360.0 * Backlights::backlightHue / 255.0);
            blArray.add(100.0 * Backlights::backlightSaturation / 255.0);
            volatileState["backlight_brightness"] = Backlights::backlightBrightness;
#if (NUM_LEDS > 6)
            volatileState["underlight_state"] = Backlights::backlightState ? "ON" : "OFF";  // backlight and underlight share the same state
            JsonArray ulArray = volatileState["underlight_hs"].to<JsonArray>();
            ulArray.add(360.0 * Backlights::underlightHue / 255.0);
            ulArray.add(100.0 * Backlights::underlightSaturation / 255.0);
            volatileState["underlight_brightness"] = Backlights::underlightBrightness;
#endif
            char buffer[384];
            size_t n = serializeJson(volatileState, buffer);
            client.publish(volatileStateTopic, 1, false, buffer);
            client.publish(persistentStateTopic, 1, false, rootConfig.toJSON().c_str());
            xSemaphoreGive(memMutex);
        } else {
            // Serial.println("Gave up waiting for memory semaphore");
        }
    }
}

void MQTTBroker::sendHADiscoveryMessage() {
    char buffer[1024];
    delay(300);

    strcpy(buffer, home);
    strcat(buffer, "/set/#");
    client.subscribe(buffer, 0);

    // This is the discovery topic for the 'screensaver' switch
    char discoveryTopic[128];
    sprintf(discoveryTopic, "homeassistant/switch/%s/screen_saver/config", id.c_str());

    JsonDocument doc;

    doc["~"] = home;
    doc["name"] = "Screen Saver";
    doc["icon"] = "mdi:sleep";
    doc["unique_id"] = "screen_saver_" + id;
    doc["stat_t"] = volatileStateTopic;
    doc["cmd_t"] = screenSaverTopic;
    doc["avty_t"] = availabilityTopic;
    doc["val_tpl"] = "{{value_json.screen_saver_on}}";
    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    size_t n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);

    sprintf(discoveryTopic, "homeassistant/number/%s/screen_saver_delay/config", id.c_str());

    doc.clear();

    doc["~"] = home;
    doc["name"] = "Screen Saver Delay";
    doc["icon"] = "mdi:knob";
    doc["unique_id"] = "screen_saver_delay" + id;
    doc["stat_t"] = "~/persistent/state";
    doc["cmd_t"] = screenSaverDelayTopic;
    doc["avty_t"] = availabilityTopic;
    doc["min"] = 0;
    doc["max"] = 120;
    doc["val_tpl"] = "{{value_json.matrix.screen_saver_delay}}";
    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);

    sprintf(discoveryTopic, "homeassistant/number/%s/brightness/config", id.c_str());

    doc.clear();

    doc["~"] = home;
    doc["name"] = "Brightness";
    doc["icon"] = "mdi:brightness-6";
    doc["unique_id"] = "brightness" + id;
    doc["stat_t"] = volatileStateTopic;
    doc["cmd_t"] = brightnessTopic;
    doc["avty_t"] = availabilityTopic;
    doc["min"] = 10;
    doc["max"] = 255;
    doc["val_tpl"] = "{{value_json.brightness}}";
    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);

    sprintf(discoveryTopic, "homeassistant/text/%s/custom/config", id.c_str());

    doc.clear();

    doc["~"] = home;
    doc["name"] = "Custom Data";
    doc["icon"] = "mdi:clock-edit";
    doc["unique_id"] = "custom-data" + id;
    doc["stat_t"] = volatileStateTopic;
    doc["cmd_t"] = customDataTopic;
    doc["avty_t"] = availabilityTopic;
    doc["val_tpl"] = "{{value_json.custom}}";
    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);

    sprintf(discoveryTopic, "homeassistant/light/%s/backlight/config", id.c_str());

    doc.clear();

    doc["~"] = home;
    doc["name"] = "Backlight";
    doc["icon"] = "mdi:led-strip-variant";
    doc["unique_id"] = "backlight" + id;

    doc["cmd_t"] = backlightStateTopic;
    doc["avty_t"] = availabilityTopic;
    doc["stat_t"] = volatileStateTopic;
    doc["stat_val_tpl"] = "{{value_json.backlight_state}}";

    doc["bri_stat_t"] = volatileStateTopic;
    doc["bri_cmd_t"] = backlightBrightnessTopic;
    doc["bri_val_tpl"] = "{{value_json.backlight_brightness}}";
    doc["hs_stat_t"] = volatileStateTopic;
    doc["hs_cmd_t"] = backlightHSTopic;
    doc["hs_val_tpl"] = "{{value_json.backlight_hs | join(',')}}";
    doc["clrm"] = true;
    JsonArray blArray = doc["sup_clrm"].to<JsonArray>();
    blArray.add("hs");
    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);

#if (NUM_LEDS > 6)
    sprintf(discoveryTopic, "homeassistant/light/%s/underlight/config", id.c_str());

    doc.clear();

    doc["~"] = home;
    doc["name"] = "Underlight";
    doc["icon"] = "mdi:led-strip-variant";
    doc["unique_id"] = "underlight" + id;

    doc["cmd_t"] = underlightStateTopic;
    doc["avty_t"] = availabilityTopic;
    doc["stat_t"] = volatileStateTopic;
    doc["stat_val_tpl"] = "{{value_json.underlight_state}}";

    doc["bri_stat_t"] = volatileStateTopic;
    doc["bri_cmd_t"] = underlightBrightnessTopic;
    doc["bri_val_tpl"] = "{{value_json.underlight_brightness}}";
    doc["hs_stat_t"] = volatileStateTopic;
    doc["hs_cmd_t"] = underlightHSTopic;
    doc["hs_val_tpl"] = "{{value_json.underlight_hs | join(',')}}";
    doc["clrm"] = true;
    JsonArray ulArray = doc["sup_clrm"].to<JsonArray>();
    ulArray.add("hs");
    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);
#endif

    sprintf(discoveryTopic, "homeassistant/select/%s/display/config", id.c_str());

    doc.clear();

    doc["~"] = home;
    doc["name"] = "Display";
    doc["icon"] = "mdi:theater";
    doc["unique_id"] = "display" + id;

    doc["cmd_t"] = displayTopic;
    doc["avty_t"] = availabilityTopic;
    doc["stat_t"] = volatileStateTopic;
    doc["val_tpl"] = "{% if value_json.display == 0 %}Time{% elif value_json.display == 1 %}Date{% elif value_json.display == 2 %}Weather{% elif value_json.display == 3 %}Slideshow{% endif %}";
    doc["command_template"] = "{% if value == 'Time' %}0{% elif value == 'Date' %}1{% elif value == 'Weather' %}2{% elif value == 'Slideshow' %}3{% endif %}";

    JsonArray optArray = doc["options"].to<JsonArray>();
    for (int i = 0; displayStates[i] != 0; i++) {
        optArray.add(displayStates[i]);
    }

    doc["dev"]["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    doc["dev"]["name"] = manifest[3];
    doc["dev"]["identifiers"][0] = WiFi.macAddress();
    doc["dev"]["model"] = manifest[0];
    doc["dev"]["sw_version"] = manifest[1];

    n = serializeJson(doc, buffer);

    client.publish(discoveryTopic, 1, false, buffer);

    client.publish(availabilityTopic, 2, true, "online");

    uint32_t msg = 1;
	xQueueSend(mainQueue, &msg, pdMS_TO_TICKS(100));
}