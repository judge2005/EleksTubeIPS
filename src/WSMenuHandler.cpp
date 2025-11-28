#include <WSMenuHandler.h>

const char* WSMenuHandler::clockMenu = "{\"1\": { \"url\" : \"clock.html\", \"title\" : \"Clock\" }}";
const char* WSMenuHandler::ledsMenu = "{\"2\": { \"url\" : \"leds.html\", \"title\" : \"LEDs\" }}";
const char* WSMenuHandler::facesMenu = "{\"3\": { \"url\" : \"faces.html\", \"title\" : \"Files\" }}";
const char* WSMenuHandler::mqttMenu = "{\"4\": { \"url\" : \"mqtt.html\", \"title\" : \"MQTT\" }}";
const char* WSMenuHandler::infoMenu = "{\"5\": { \"url\" : \"info.html\", \"title\" : \"Info\" }}";
const char* WSMenuHandler::networkMenu = "{\"6\": { \"url\" : \"network.html\", \"title\" : \"Network\" }}";
const char* WSMenuHandler::weatherMenu = "{\"7\": { \"url\" : \"weather.html\", \"title\" : \"Weather\" }}";
const char* WSMenuHandler::matrixMenu = "{\"8\": { \"url\" : \"matrix.html\", \"title\" : \"Screen Saver\" }}";

void WSMenuHandler::handle(AsyncWebSocketClient *client, char *data) {
	String json("{\"type\":\"sv.init.menu\", \"value\":[");
	const char *sep = "";
	for (int i=0; items[i] != 0; i++) {
		json.concat(sep);json.concat(items[i]);sep=",";
	}
	json.concat("]}");
	client->text(json);
}

void WSMenuHandler::setItems(const char **items) {
	this->items = items;
}

