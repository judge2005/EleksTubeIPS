#include <WSConfigHandler.h>

void WSConfigHandler::handle(AsyncWebSocketClient *client, char *data) {
	client->text(getData(data));
}

void WSConfigHandler::broadcast(AsyncWebSocket &ws, char *data) {
	ws.textAll(getData(data));
}

String WSConfigHandler::getData(char *data) {
	String json("{\"type\":\"sv.init.");
	json.concat(name);
	json.concat("\", \"value\":{");
    BaseConfigItem *clockConfig = rootConfig.get(name);
    char *sep = "";

    if (clockConfig != 0) {
        json.concat(sep);
        json.concat(clockConfig->toJSON(true));
    }

	if (cbFunc != NULL) {
		json.concat(sep);
		json.concat(cbFunc());
	}

	json.concat("}}");

	return json;
}

