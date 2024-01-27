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
        sendProvisioned(pendingResponse);
        pendingResponse = improv::Command::UNKNOWN;
    }

    if (millis() - pendingCmdReceived > 10000) {
      tfts->setStatus("WiFI Timed Out");
      pendingResponse = improv::Command::UNKNOWN;
      set_state(improv::STATE_STOPPED);
      set_error(improv::Error::ERROR_UNABLE_TO_CONNECT);
    }

    return;
  }

  while (Serial.available() > 0) {
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

bool ImprovWiFi::onCommandCallback(improv::ImprovCommand cmd) {
  // char statusBuf[20];
  // sprintf(statusBuf, "Got command %d", cmd.command);

  IMPROV_DEBUG(statusBuf);
  switch (cmd.command) {
    case improv::Command::GET_CURRENT_STATE:
    {
      if ((WiFi.status() == WL_CONNECTED)) {
        tfts->setStatus("Improv Connected");
        sendProvisioned(improv::Command::GET_CURRENT_STATE);
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

void ImprovWiFi::sendProvisioned(improv::Command responseTo) {
  set_state(improv::STATE_PROVISIONED);

  uint32_t ip = WiFi.localIP();
  char szRet[30];
  sprintf(szRet,"http://%u.%u.%u.%u/", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, ip >> 24);

  std::vector<std::string> urls = {
      szRet
  };

  std::vector<uint8_t> data = improv::build_rpc_response(responseTo, urls, false);

  send_response(data);

  tfts->setStatus("Provisioned");
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
  // Need to send line-feed as first byte so that client can flush input buffers up until this packet
  uint8_t data[] = {10, 'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION, improv::TYPE_CURRENT_STATE, 1, state, 0};

  uint8_t checksum = 0x00;
  for (int i=1; i<sizeof(data)-1; i++)  // skip first and last bytes
    checksum += data[i];
  data[sizeof(data)-1] = checksum;

  Serial.write(data, sizeof(data));

  IMPROV_DEBUG("Sent state");  
}

void ImprovWiFi::send_response(std::vector<uint8_t> &response) {
  // Need to send line-feed as first byte so that client can flush input buffers up until this packet
  std::vector<uint8_t> data = {10, 'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION, improv::TYPE_RPC_RESPONSE, (uint8_t)response.size()};
  data.insert(data.end(), response.begin(), response.end());

  uint8_t checksum = 0x00;
  for (int i=1; i<data.size(); i++)
    checksum += data[i];
  data.push_back(checksum);

  Serial.write(data.data(), data.size());

  IMPROV_DEBUG("Response sent");
}

void ImprovWiFi::set_error(improv::Error error) {
  // Need to send line-feed as first byte so that client can flush input buffers up until this packet
  uint8_t data[] = {10, 'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION, improv::TYPE_ERROR_STATE, 1, error, 0};

  uint8_t checksum = 0x00;
  for (int i=1; i<sizeof(data)-1; i++)  // skip first and last bytes
    checksum += data[i];
  data[sizeof(data)-1] = checksum;

  Serial.write(data, sizeof(data));

  IMPROV_DEBUG("Error sent");
}