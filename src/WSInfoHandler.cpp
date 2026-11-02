#include <WSInfoHandler.h>
// #include <Uptime.h>
#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
extern "C" {
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_heap_caps.h"
}

static uint32_t sketchSize(sketchSize_t response) {
    // Verifying the image reads and hashes all of it, which takes a while,
    // and the running image can't change until the next restart
    static uint32_t imageLen = 0;
    static uint32_t partitionSize = 0;

    if (partitionSize == 0) {
        esp_image_metadata_t data;
        const esp_partition_t *running = esp_ota_get_running_partition();
        if (!running) return 0;
        const esp_partition_pos_t running_pos  = {
            .offset = running->address,
            .size = running->size,
        };
        data.start_addr = running_pos.offset;
        esp_image_verify(ESP_IMAGE_VERIFY, &running_pos, &data);
        imageLen = data.image_len;
        partitionSize = running_pos.size;
    }

    if (response) {
        return partitionSize - imageLen;
    } else {
        return imageLen;
    }
}
    
void WSInfoHandler::handle(AsyncWebSocketClient *client, char *data) {
	cbFunc();

	// static Uptime uptime;
	JsonDocument doc;
	JsonObject root = doc.to<JsonObject>();

	root["type"] = "sv.init.info";
	size_t freeHeap = ESP.getFreeHeap();
	size_t free8bitHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	size_t freeIRAMHeap = freeHeap - free8bitHeap;

	JsonVariant value = root["value"].to<JsonObject>();
	value["esp_free_iram_heap"] = freeIRAMHeap;
	value["esp_free_heap"] = free8bitHeap;
	value["esp_free_heap_min"] = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
	value["esp_max_alloc_heap"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
	value["esp_sketch_size"] = sketchSize(SKETCH_SIZE_TOTAL);
	value["esp_sketch_space"] = sketchSize(SKETCH_SIZE_FREE);

	value["esp_chip_id"] = String(ESP.getChipRevision(), HEX);

	value["wifi_ip_address"] = WiFi.localIP().toString();
	value["wifi_mac_address"] = WiFi.macAddress();
	value["wifi_ssid"] = WiFi.SSID();
	value["wifi_ap_ssid"] = ssid;
	value["mqtt_id"] = mqttId;
	value["hostname"] = hostname;
	value["software_revision"] = revision;
	value["description"] = description;

    value["fs_size"] = fsSize;
    value["fs_free"] = fsFree;
	value["brightness"] = brightness;
	value["triggered"] = triggered;
	value["clock_on"] = clockOn;

	value["up_time"] = uptime;
	value["sync_time"] = lastUpdateTime;
	value["sync_failed_msg"] = lastFailedMessage;
	value["sync_failed_cnt"] = failedCount;

	// if (pBlankingMonitor) {
	// 	value["on_time"] = pBlankingMonitor->onTime();
	// 	value["off_time"] = pBlankingMonitor->offTime();
	// }

	String serializedJSON;
	serializeJson(root, serializedJSON);
	client->text(serializedJSON);
}


