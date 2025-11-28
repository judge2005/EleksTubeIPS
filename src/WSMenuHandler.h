#ifndef WSMENUHANDLER_H_
#define WSMENUHANDLER_H_

#include <Arduino.h>
#include <WString.h>
#include <WSHandler.h>

class WSMenuHandler : public WSHandler {
public:
	WSMenuHandler(const char **items) : items(items) { }
	virtual void handle(AsyncWebSocketClient *client, char *data);
	void setItems(const char **items);

	static const char* clockMenu;
	static const char* ledsMenu;
	static const char* facesMenu;
	static const char* weatherMenu;
	static const char* matrixMenu;
	static const char* mqttMenu;
	static const char* networkMenu;
	static const char* infoMenu;

private:
	const char **items;
};


#endif /* WSMENUHANDLER_H_ */
