#ifndef ELEKSTUBE_MQTT_H
#define ELEKSTUBE_MQTT_H
#include <ConfigItem.h>
#ifdef ASYNC_MQTT_HA_CLIENT
#include <AsyncMqttClient.h>
#else
#include <espMqttClient.h>
#endif
#include "IRAMPtrArray.h"

class MQTTBroker
{
public:
    static StringConfigItem& getHost() { static StringConfigItem mqtt_host("mqtt_host", 25, ""); return mqtt_host; }
    static IntConfigItem& getPort() { static IntConfigItem mqtt_port("mqtt_port", 1883); return mqtt_port; }
    static StringConfigItem& getUser() { static StringConfigItem mqtt_user("mqtt_user", 25, ""); return mqtt_user; }
    static StringConfigItem& getPassword() { static StringConfigItem mqtt_password("mqtt_password", 25, ""); return mqtt_password; }

    bool init(const String& id);
    void connect();
    void checkConnection();
    void publishState();

private:
    void onConnect(bool sessionPresent);
#ifdef ASYNC_MQTT_HA_CLIENT
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length);
#else
    void onDisconnect(espMqttClientTypes::DisconnectReason reason);
    void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length);
#endif

    void sendHADiscoveryMessage();

    String id;
    char home[40];
    char persistentStateTopic[64];
    char volatileStateTopic[64];
    char availabilityTopic[64];

    const char* screenSaverTopic = "~/set/screen_saver";
    const char* brightnessTopic = "~/set/brightness";
    const char* screenSaverDelayTopic = "~/set/screen_saver_delay";
    const char* displayTopic = "~/set/display";

    const char* customDataTopic = "~/set/custom";

    const char* backlightHSTopic = "~/set/backlight_hs";
    const char* backlightStateTopic = "~/set/backlight_state";
    const char* backlightBrightnessTopic = "~/set/backlight_brightness";

    const char* underlightHSTopic = "~/set/underlight_hs";
    const char* underlightStateTopic = "~/set/underlight_state";
    const char* underlightBrightnessTopic = "~/set/underlight_brightness";

    static IRAMPtrArray<const char*> displayStates;

    bool reconnect = false;
    uint32_t lastReconnect = 0;

#ifdef ASYNC_MQTT_HA_CLIENT
    AsyncMqttClient client;
#else
    espMqttClient client;
#endif
};
#endif