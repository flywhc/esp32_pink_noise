#ifndef HOST_CHECK_HANDLER_H
#define HOST_CHECK_HANDLER_H

#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "config.h"

class HostCheckHandler: public AsyncWebHandler {
public:
    HostCheckHandler(bool (*isAPMode)()) : _isAPMode(isAPMode) {}

    virtual bool canHandle(AsyncWebServerRequest *request) const override final {
        if(_isAPMode()) {
            String host = request->host();
            if(host.compareTo(HOSTNAME) != 0) {
                Serial.println("AP mode received request with host: " + host);
                Serial.println("Redirect to: http://" HOSTNAME);
                return true;
            }
        }
        return false;
    }

    virtual void handleRequest(AsyncWebServerRequest *request) override final {
        String host = request->host();
        request->redirect("http://" HOSTNAME);
    }

private:
    bool (*_isAPMode)();
};

#endif // HOST_CHECK_HANDLER_H 