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
		name(name),
		excludes(NULL) {
	}

	// excludes are the names of settings to leave out, ending with 0
	WSConfigHandler(BaseConfigItem& rootConfig, const char *name, CbFunc cbFunc, const char **excludes = NULL) :
		cbFunc(cbFunc),
		rootConfig(rootConfig),
		name(name),
		excludes(excludes) {
	}

	virtual void handle(AsyncWebSocketClient *client, char *data);
	virtual void broadcast(AsyncWebSocket &ws, char *data);

private:
	CbFunc cbFunc;

	String getData(char *data);
	
	BaseConfigItem& rootConfig;
	const char *name;
	const char **excludes;
};

#endif /* WSCONFIGHANDLER_H_ */
