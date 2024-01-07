#ifndef WSCONFIGHANDLER_H_
#define WSCONFIGHANDLER_H_

#include <ConfigItem.h>
#include <WSHandler.h>

class WSConfigHandler: public WSHandler {
public:
	typedef String (*CbFunc)();

	WSConfigHandler(BaseConfigItem& rootConfig, const char *name) :
		cbFunc(NULL),
		rootConfig(rootConfig),
		name(name) {
	}

	WSConfigHandler(BaseConfigItem& rootConfig, const char *name, CbFunc cbFunc) :
		cbFunc(cbFunc),
		rootConfig(rootConfig),
		name(name) {
	}

	virtual void handle(AsyncWebSocketClient *client, char *data);
	virtual void broadcast(AsyncWebSocket &ws, char *data);

private:
	CbFunc cbFunc;

	String getData(char *data);
	
	BaseConfigItem& rootConfig;
	const char *name;
};

#endif /* WSCONFIGHANDLER_H_ */
