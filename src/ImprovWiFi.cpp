#include <WiFi.h>
#include "ImprovWiFi.h"
#include "TFTs.h"

//#define IMPROV_DEBUG(...) { tfts->printlnAll(__VA_ARGS__); }
#ifndef IMPROV_DEBUG
#define IMPROV_DEBUG(...) {  }
#endif

void ImprovWiFi::loop() {
  if (pendingResponse == improv::Command::WIFI_SETTINGS) {
    if (WiFi.status() == WL_CONNECTED) {
        pendingResponse = improv::Command::UNKNOWN;
        sendProvisioned();
    }

    if (millis() - pendingCmdReceived > 10000) {
      tfts->setStatus("WiFI Timed Out");
      pendingResponse = improv::Command::UNKNOWN;
      set_state(improv::STATE_STOPPED);
      set_error(improv::Error::ERROR_UNABLE_TO_CONNECT);
    }

    return;
  }

  if (Serial.available() > 0) {
    uint8_t b = Serial.read();
    if (x_position < sizeof(x_buffer)) {
      if (parse_improv_serial_byte(
          x_position,
          b,
          x_buffer,
          std::bind(&ImprovWiFi::onCommandCallback, this, std::placeholders::_1),
          std::bind(&ImprovWiFi::onErrorCallback, this, std::placeholders::_1)))
      {
        x_buffer[x_position++] = b;      
      } else {
        sprintf(msgBuf, "Extra byte %02x", b);
        IMPROV_DEBUG(msgBuf);
        x_position = 0;
      }
    } else {
      IMPROV_DEBUG("Buffer overflow");
      x_position = 0;
    }
  }
}

void ImprovWiFi::sendProvisioned() {
    set_state(improv::STATE_PROVISIONED);
    String localURL = "http://" + WiFi.localIP().toString();
    std::vector<std::string> urls = {
        localURL.c_str()
    };

    std::vector<uint8_t> data = improv::build_rpc_response(improv::WIFI_SETTINGS, urls, false);
    send_response(data);
}

bool ImprovWiFi::onCommandCallback(improv::ImprovCommand cmd) {
  // char statusBuf[20];
  // sprintf(statusBuf, "Got command %d", cmd.command);

  IMPROV_DEBUG(statusBuf);
  switch (cmd.command) {
    case improv::Command::GET_CURRENT_STATE:
    {
      if ((WiFi.status() == WL_CONNECTED)) {
        sendProvisioned();
      } else {
        tfts->setStatus("Provisioning");
        set_state(improv::State::STATE_AUTHORIZED);
      }
      
      break;
    }

    case improv::Command::WIFI_SETTINGS:
    {
      if (cmd.ssid.length() == 0) {
        set_error(improv::Error::ERROR_INVALID_RPC);
        break;
      }
     
      set_state(improv::STATE_PROVISIONING);
      
      ssid = cmd.ssid;
      password = cmd.password;
      pendingResponse = cmd.command;
      pendingCmdReceived = millis();
      tfts->setStatus("Connect WiFi");
      // Don't try connecting. wifiManager should do that asynchronously 
#ifdef notdef
      if (connectWifi(cmd.ssid, cmd.password)) {
        set_state(improv::STATE_PROVISIONED);        
        std::vector<uint8_t> data = improv::build_rpc_response(improv::WIFI_SETTINGS, getLocalUrl(), false);
        send_response(data);
      } else {
        set_state(improv::STATE_STOPPED);
        set_error(improv::Error::ERROR_UNABLE_TO_CONNECT);
      }
#endif      
      break;
    }

    case improv::Command::GET_DEVICE_INFO:
    {
      std::vector<std::string> infos = {
        // Firmware name
        firmwareName,
        // Firmware version
        version,
        // Hardware chip/variant
        chip,
        // Device name
        deviceName
      };
      std::vector<uint8_t> data = improv::build_rpc_response(improv::GET_DEVICE_INFO, infos, false);
      send_response(data);
      break;
    }

    case improv::Command::GET_WIFI_NETWORKS:
    {
      getAvailableWifiNetworks();
      break;
    }

    default: {
      set_error(improv::ERROR_UNKNOWN_RPC);
      return false;
    }
  }

  return true;
}

void ImprovWiFi::getAvailableWifiNetworks() {
  int networkNum = WiFi.scanNetworks();

  for (int id = 0; id < networkNum; ++id) { 
    std::vector<uint8_t> data = improv::build_rpc_response(
            improv::GET_WIFI_NETWORKS, {WiFi.SSID(id), String(WiFi.RSSI(id)), (WiFi.encryptionType(id) == WIFI_AUTH_OPEN ? "NO" : "YES")}, false);
    send_response(data);
    delay(1);
  }
  //final response
  std::vector<uint8_t> data =
          improv::build_rpc_response(improv::GET_WIFI_NETWORKS, std::vector<std::string>{}, false);
  send_response(data);
}

void ImprovWiFi::onErrorCallback(improv::Error err) {
    sprintf(msgBuf, "Improv Err %d", err);
    tfts->setStatus(msgBuf);
}

void ImprovWiFi::set_state(improv::State state) {  
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(11);
  data[6] = improv::IMPROV_SERIAL_VERSION;
  data[7] = improv::TYPE_CURRENT_STATE;
  data[8] = 1;
  data[9] = state;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[10] = checksum;

  Serial.write(data.data(), data.size());
  IMPROV_DEBUG("Sent state");  
}

void ImprovWiFi::send_response(std::vector<uint8_t> &response) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(9);
  data[6] = improv::IMPROV_SERIAL_VERSION;
  data[7] = improv::TYPE_RPC_RESPONSE;
  data[8] = response.size();
  data.insert(data.end(), response.begin(), response.end());

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data.push_back(checksum);

  Serial.write(data.data(), data.size());
  IMPROV_DEBUG("Response sent");
}

void ImprovWiFi::set_error(improv::Error error) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(11);
  data[6] = improv::IMPROV_SERIAL_VERSION;
  data[7] = improv::TYPE_ERROR_STATE;
  data[8] = 1;
  data[9] = error;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[10] = checksum;

  Serial.write(data.data(), data.size());
  IMPROV_DEBUG("Error sent");
}