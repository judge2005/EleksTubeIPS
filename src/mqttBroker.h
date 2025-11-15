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

    const char* screenSaverTopic = "~/screen_saver/set";
    const char* brightnessTopic = "~/brightness/set";
    const char* screenSaverDelayTopic = "~/screen_saver_delay/set";

    const char* customDataTopic = "~/custom/set";

    const char* backlightHSTopic = "~/backlight_hs/set";
    const char* backlightStateTopic = "~/backlight_state/set";
    const char* backlightBrightnessTopic = "~/backlight_brightness/set";

    const char* underlightHSTopic = "~/underlight_hs/set";
    const char* underlightStateTopic = "~/underlight_state/set";
    const char* underlightBrightnessTopic = "~/underlight_brightness/set";

    bool reconnect = false;
    uint32_t lastReconnect = 0;

    AsyncMqttClient client;
};
#endif