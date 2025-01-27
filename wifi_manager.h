#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#include "ElegantOTA.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPmDNS.h>
#include "web_content.h"
#include "HostCheckHandler.h"
#include "config.h"

enum WifiState {
    WIFI_IDLE,
    WIFI_START,
    WIFI_RESCAN,
    WIFI_COMPLETE
};

const IPAddress apIP(192, 168, 4, 1);

class WifiManager {
private:
    static const int DNS_PORT = 53;
    WifiState wifiState = WIFI_IDLE;
    
    AsyncWebServer server;
    DNSServer dnsServer;
    const std::vector<std::string>* btDevices;
    std::function<void(WifiState, const char*)> onWifiStateChanged;

public:
    WifiManager() : server(80) {}

    void start(const std::vector<std::string>* devices, std::function<void(WifiState, const char*)> callback) {
        wifiState = WIFI_START;
        btDevices = devices;
        onWifiStateChanged = callback;
        
        Serial.println("Start WiFi AP mode");
        
        // start AP mode without password
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(HOSTNAME);

        // mDNS register service in LAN
        if (MDNS.begin(HOSTNAME)) {
            MDNS.addService("http", "tcp", 80); 
        }

        dnsServer.start(DNS_PORT, "*", apIP);
        setupRoutes();
        server.begin();
    }

    void stop() {
        delay(100);
        server.end();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        wifiState = WIFI_IDLE;
    }

    void update() {
        dnsServer.processNextRequest();
    }

    void updateDevices(const std::vector<std::string>* devices) {
        btDevices = devices;
        Serial.print("Updated devices count: ");
        Serial.println(btDevices->size());
    }

    WifiState getState() const {
        return wifiState;
    }

private:
    void setupRoutes() {
        server.addHandler(new HostCheckHandler([]() { return true; }));
        ElegantOTA.begin(&server);
         ElegantOTA.setAutoReboot(true);
        // handle all unknown domain requests, implement captive portal
        server.onNotFound([](AsyncWebServerRequest *request){
            request->redirect("/");
        });

        // provide HTML page
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", INDEX_HTML);
            response->addHeader("Content-Language", "zh-CN");
            request->send(response);
        });

        // provide device list API
        server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest *request){
            Serial.println("/api/devices");
            AsyncResponseStream *response = request->beginResponseStream("application/json");
            DynamicJsonDocument doc(1024);
            
            if (btDevices->empty()) {
                doc["status"] = "empty";
                doc["message"] = "Please turn on the bluetooth speaker and enable the pairing state, click the refresh button above, wait for the white light to stop flashing, and then reconnect to this WIFI hotspot";
            } else {
                doc["status"] = "ok";
                JsonArray array = doc.createNestedArray("devices");
                for(const auto& device : *btDevices) {
                    array.add(device);
                }
            }
            
            serializeJson(doc, *response);
            request->send(response);
        });

        server.on("/api/rescan", HTTP_POST, [this](AsyncWebServerRequest *request){
            Serial.println("onRescan requested");
            request->send(200, "application/json", "{\"status\":\"ok\"}");
            wifiState = WIFI_RESCAN;
            onWifiStateChanged(wifiState, nullptr);
        });

        // device selection
        AsyncCallbackJsonWebHandler* selectHandler = new AsyncCallbackJsonWebHandler(
            "/api/select",
            [this](AsyncWebServerRequest *request, JsonVariant &json) {
                Serial.println("select requested");
                if (json.is<JsonObject>()) {
                    JsonObject jsonObj = json.as<JsonObject>();
                    if (jsonObj.containsKey("device")) {
                        const char* deviceName = jsonObj["device"].as<const char*>();
                        Serial.print("Selected device: ");
                        Serial.println(deviceName);
                        
                        request->send(200, "application/json", "{\"status\":\"ok\"}");
                        wifiState = WIFI_COMPLETE;
                        onWifiStateChanged(wifiState, deviceName);
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing device parameter\"}");
                    }
                } else {
                    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}");
                }
            }
        );

        server.addHandler(selectHandler);
        
    }
};

#endif 