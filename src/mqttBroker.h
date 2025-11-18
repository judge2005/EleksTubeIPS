#ifndef ELEKSTUBE_MQTT_H
#define ELEKSTUBE_MQTT_H
#include <ConfigItem.h>
#include <AsyncMqttClient.h>

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
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total_length);

    void sendHADiscoveryMessage();

    String id;
    char home[40];
    char persistentStateTopic[64];
    char volatileStateTopic[64];
    char availabilityTopic[64];

    const char* screenSaverTopic = "~/set/screen_saver";
    const char* brightnessTopic = "~/set/brightness";
    const char* screenSaverDelayTopic = "~/set/screen_saver_delay";

    const char* customDataTopic = "~/set/custom";

    const char* backlightHSTopic = "~/set/backlight_hs";
    const char* backlightStateTopic = "~/set/backlight_state";
    const char* backlightBrightnessTopic = "~/set/backlight_brightness";

    const char* underlightHSTopic = "~/set/underlight_hs";
    const char* underlightStateTopic = "~/set/underlight_state";
    const char* underlightBrightnessTopic = "~/set/underlight_brightness";

    bool reconnect = false;
    uint32_t lastReconnect = 0;

    AsyncMqttClient client;
};
#endif