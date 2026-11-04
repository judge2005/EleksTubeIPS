#ifndef ELEKSTUBE_MQTT_H
#define ELEKSTUBE_MQTT_H
#include <ConfigItem.h>
#include <ArduinoJson.h>
#include <FS.h>
#ifdef ASYNC_MQTT_HA_CLIENT
#include <AsyncMqttClient.h>
#else
#include <espMqttClient.h>
#endif
#include "IRAMPtrArray.h"

struct Setting;

class MQTTBroker
{
public:
    static StringConfigItem& getHost() { static StringConfigItem mqtt_host("mqtt_host", 25, ""); return mqtt_host; }
    static IntConfigItem& getPort() { static IntConfigItem mqtt_port("mqtt_port", 1883); return mqtt_port; }
    static StringConfigItem& getUser() { static StringConfigItem mqtt_user("mqtt_user", 25, ""); return mqtt_user; }
    static StringConfigItem& getPassword() { static StringConfigItem mqtt_password("mqtt_password", 25, ""); return mqtt_password; }

    MQTTBroker();

    bool init(const String& id);
    void connect();
    void checkConnection();
    bool publishState();
    bool sendHADiscoveryMessage();
    void sendFileDiscovery(const String& dir);

private:
    void onConnect(bool sessionPresent);
#ifdef ASYNC_MQTT_HA_CLIENT
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length);
#else
    void onDisconnect(espMqttClientTypes::DisconnectReason reason);
    void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length);
#endif

    void publishDiscovery(const char *component, const char *objectId, JsonDocument &doc, const char *uniqueId = 0);
    void sendSettingDiscovery(const Setting &setting, JsonDocument &doc);
    void setSetting(const char *topic, const char *value);
    void setConfigItem(BaseConfigItem &item, const char *value);
    void setConfigNumber(BaseConfigItem &item, const char *value, int min, int max);
    void setConfigOption(BaseConfigItem &item, const char *value, const char *const *options);
    void setConfigCoordinate(BaseConfigItem &item, const char *value, int min, int max);

    void onFileMessage(const char *path, const uint8_t *payload, size_t length, size_t index, size_t total_length);
    void startFileMessage(const char *path, const uint8_t *payload, size_t length, size_t total_length);
    void publishFileStatus();

    String id;
    char home[40];
    char persistentStateTopic[64];
    char volatileStateTopic[64];
    char availabilityTopic[64];
    char fileStatusTopic[64];

    bool deviceDescribed = false;   // Whether this round of discovery messages has described the device yet

    const char* screenSaverTopic = "~/set/screen_saver";
    const char* brightnessTopic = "~/set/brightness";
    // Not ~/set/screen_saver, the config item's name, because the screen saver switch already uses that
    const char* screenSaverModeTopic = "~/set/screen_saver_mode";
    const char* displayTopic = "~/set/display";

    const char* customDataTopic = "~/set/custom";

    const char* backlightHSTopic = "~/set/backlight_hs";
    const char* backlightStateTopic = "~/set/backlight_state";
    const char* backlightBrightnessTopic = "~/set/backlight_brightness";

    const char* underlightHSTopic = "~/set/underlight_hs";
    const char* underlightStateTopic = "~/set/underlight_state";
    const char* underlightBrightnessTopic = "~/set/underlight_brightness";

    const char* restartTopic = "~/set/restart";

    // ~/files/<upload|delete>/<faces|weather|slides>/<name>
    const char* filesTopic = "~/files/";

    static IRAMPtrArray<const char*> displayStates;

    // The file upload or delete currently being received
    String fileAction;
    String fileDir;
    String fileName;
    const char *fileError = 0;
    fs::File uploadFile;

    bool reconnect = false;
    uint32_t lastReconnect = 0;

#ifdef ASYNC_MQTT_HA_CLIENT
    AsyncMqttClient client;
#else
    espMqttClient client;
#endif
};
#endif