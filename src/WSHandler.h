#ifndef WSHANDLER_H_
#define WSHANDLER_H_
#include <ESPAsyncWebServer.h>

class WSHandler {
public:
	virtual void handle(AsyncWebSocketClient *client, char *data) = 0;
};


#endif /* WSHANDLER_H_ */