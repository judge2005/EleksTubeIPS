#ifndef WSMENUHANDLER_H_
#define WSMENUHANDLER_H_

#include <Arduino.h>
#include <WString.h>
#include <WSHandler.h>

class WSMenuHandler : public WSHandler {
public:
	WSMenuHandler(String **items) : items(items) { }
	virtual void handle(AsyncWebSocketClient *client, char *data);
	void setItems(String **items);

	static String clockMenu;
	static String ledsMenu;
	static String facesMenu;
	static String presetsMenu;
	static String syncMenu;
	static String infoMenu;
	static String presetNamesMenu;

private:
	String **items;
};


#endif /* WSMENUHANDLER_H_ */
