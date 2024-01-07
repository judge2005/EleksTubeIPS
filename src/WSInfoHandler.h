#ifndef WSINFOHANDLER_H_
#define WSINFOHANDLER_H_

#include <WSHandler.h>
// #include <BlankTimeMonitor.h>

class WSInfoHandler : public WSHandler {
public:
	typedef void (*CbFunc)();

	WSInfoHandler(CbFunc cbFunc) : cbFunc(cbFunc)
        // , pBlankingMonitor(NULL)
    {
	}

	virtual void handle(AsyncWebSocketClient *client, char *data);

    void setFSFree(const String& free) {
        fsFree = free;
    }
    void setFSSize(const String& size) {
        fsSize = size;
    }
    
	void setFailedCount(const String& failedCount) {
		this->failedCount = failedCount;
	}

	void setBrightness(const String& brightness) {
		this->brightness = brightness;
	}

	void setClockOn(const String& clockOn) {
		this->clockOn = clockOn;
	}

	void setTriggered(const String& triggered) {
		this->triggered = triggered;
	}

	void setLastFailedMessage(const String& lastFailedMessage) {
		this->lastFailedMessage = lastFailedMessage;
	}

	void setLastUpdateTime(const String& lastUpdateTime) {
		this->lastUpdateTime = lastUpdateTime;
	}

	// void setBlankingMonitor(BlankTimeMonitor* blankingMonitor) {
	// 	pBlankingMonitor = blankingMonitor;
	// }

	void setSsid(const String& ssid) {
		this->ssid = ssid;
	}

	void setHostname(const String& hostname) {
		this->hostname = hostname;
	}

	void setRevision(const String& revision) {
		this->revision = revision;
	}

private:
	CbFunc cbFunc;

	// BlankTimeMonitor *pBlankingMonitor;
	String ssid;
    String fsSize;
    String fsFree;
	String brightness;
	String clockOn;
	String triggered;
	String lastUpdateTime;
	String lastFailedMessage;
	String failedCount;
	String hostname;
	String revision;
};


#endif /* WSINFOHANDLER_H_ */
