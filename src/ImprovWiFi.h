#ifndef _IMPROV_WIFI_H
#define _IMPROV_WIFI_H

#include "improv.h"

class ImprovWiFi {
public:
    ImprovWiFi(const char *firmwareName, const char *version, const char *chip, const char *deviceName) :
        firmwareName(firmwareName), version(version), chip(chip), deviceName(deviceName), ssid(""), password(""), pendingResponse(improv::Command::UNKNOWN) {
    }

    void loop();

    const char* getSSID() { return ssid.c_str(); }
    const char* getPassword() { return password.c_str(); };
    void clearCredentials() { ssid = ""; password = ""; }
private:
    void getAvailableWifiNetworks();
    void sendProvisioned();

    void set_state(improv::State state);
    void send_response(std::vector<uint8_t> &response);
    void set_error(improv::Error error);

    bool onCommandCallback(improv::ImprovCommand cmd);
    void onErrorCallback(improv::Error err);

    char msgBuf[16];
    uint8_t x_buffer[50];
    uint8_t x_position = 0;
    const char *firmwareName, *version, *chip, *deviceName;

    improv::Command pendingResponse;
    unsigned long pendingCmdReceived;

    std::string ssid;
    std::string password;
};

#endif