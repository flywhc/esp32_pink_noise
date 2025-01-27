#include "button_handler.h"
#include "audio_player.h"
#include "wifi_manager.h"
#include <Preferences.h>
#include "config.h"


Preferences preferences;
ButtonHandler buttonHandler;
AudioPlayer audioPlayer;

AsyncWebServer* server = nullptr;

WifiManager wifiManager;

// device state enum
enum DeviceState {
    STATE_IDLE,          // idle state
    STATE_PLAYING,       // normal playing state
    STATE_SCAN_START,    // start scanning
    STATE_SCAN_WAITING,  // waiting for scanning to complete
    STATE_WIFI_START,    // start WiFi
    STATE_WIFI_RUNNING,  // WiFi running
    STATE_WIFI_RESCAN,  // WiFi running
    STATE_SCAN_COMPLETE  // scanning complete
};

// global variables
DeviceState deviceState = STATE_PLAYING;
unsigned long scanStartTime = 0;
const unsigned long SCAN_TIMEOUT = 15000;  // 15 seconds scan timeout

// button callback functions
void onVolumeUp() {
    if (deviceState == STATE_PLAYING) {
        int newVolume = audioPlayer.getVolume() + volumeStep;
        if(newVolume > 100) newVolume = 100;
        audioPlayer.setVolume(newVolume);
        preferences.putInt("Vol", newVolume);
        preferences.end();
        preferences.begin(prefKey, false);
        Serial.print("Vol Up: ");
        Serial.println(newVolume);
    }
}

void onVolumeDown() {
    if (deviceState == STATE_PLAYING) {
        int newVolume = audioPlayer.getVolume() - volumeStep;
        if(newVolume < 0) newVolume = 0;
        audioPlayer.setVolume(newVolume);
        preferences.putInt("Vol", newVolume);
        preferences.end();
        preferences.begin(prefKey, false);
        Serial.print("Vol Down: ");
        Serial.println(newVolume);
    }
}

void onMute() {
    if (deviceState == STATE_PLAYING) {
        audioPlayer.togglePlay();
        Serial.println(audioPlayer.getIsPlaying() ? "Unmute" : "Mute");
    }
}

void onNext() {
    if (deviceState == STATE_PLAYING) {
        audioPlayer.nextAlgorithm();
        switch(audioPlayer.getCurrentAlgorithm()) {
            case 0: Serial.println("0. Pink filter v2"); break;
            case 1: Serial.println("1. Brown"); break;
            default: Serial.println("2. Pink cursor"); break;
        }
    }
}

unsigned int blinkTime = 0;
void handleDeviceState() {
    if(deviceState == STATE_IDLE) {
        blinkTime ++;
        digitalWrite(LED_PIN, (blinkTime % 5) == 0);
    }
    if (deviceState == STATE_PLAYING) {
        return;
    }

    unsigned long currentTime = millis();
    
    switch(deviceState) {
        case STATE_SCAN_START:
            Serial.println("Scanning bluetooth devices");
            digitalWrite(LED_PIN, 1);
            audioPlayer.startScan();
            scanStartTime = currentTime;
            deviceState = STATE_SCAN_WAITING;
            break;
            
        case STATE_SCAN_WAITING:
            digitalWrite(LED_PIN, ((currentTime - scanStartTime) / 500) % 2);
            
            if (currentTime - scanStartTime >= SCAN_TIMEOUT) {
                auto devices = audioPlayer.getDevices();
                Serial.print("Found devices: ");
                Serial.println(devices->size());
                
                audioPlayer.stop();
                
                digitalWrite(LED_PIN, HIGH);
                deviceState = STATE_WIFI_START;
            }
            break;
            
        case STATE_WIFI_START:
            {
                Serial.println("wifi start");
                auto devices = audioPlayer.getDevices();
                Serial.println("wifi start2");
                wifiManager.start(devices, [](WifiState wifiState, const char* deviceName) {
                    switch(wifiState) {
                        case WIFI_RESCAN: deviceState = STATE_WIFI_RESCAN; break;
                        case WIFI_COMPLETE:
                            {
                                deviceState = STATE_SCAN_COMPLETE;
                                Serial.println(String("Select device: ") + deviceName);
                                preferences.putString("btdev", deviceName);
                                preferences.end();
                                delay(500);
                                preferences.begin(prefKey, false);
                                char buf[20];
                                strcpy(buf, defaultBtName);
                                String deviceName1 = preferences.getString("btdev", buf);
                                if(deviceName1.compareTo(deviceName) != 0) Serial.println("error: deviceName1 not same");
                                esp_restart();
                            }
                            break;
                        default: deviceState = STATE_WIFI_RUNNING; break;
                    }
                });
                deviceState = STATE_WIFI_RUNNING;
            }
            break;
        case STATE_WIFI_RUNNING:
            ElegantOTA.loop();
            break;

        case STATE_WIFI_RESCAN:
            Serial.println("Stop WiFi before rescan");
            wifiManager.stop();
            delay(100);
            deviceState = STATE_SCAN_START;
            break;
            
        case STATE_SCAN_COMPLETE:
            Serial.println("Stop WiFi after device selected");
            wifiManager.stop();
            digitalWrite(LED_PIN, LOW);
            
            audioPlayer.init(preferences.getString("btdev", defaultBtName).c_str());
            
            deviceState = STATE_PLAYING;
            break;
    }
}

void onAllButtons() {
    if (deviceState == STATE_PLAYING) {
        deviceState = STATE_SCAN_START;
    }
}

char buf[40];
void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, 1);
    Serial.println("Device started");
    setCpuFrequencyMhz(160);
    preferences.begin(prefKey, false);
    
    // initialize button handler
    buttonHandler.init();
    buttonHandler.setCallbacks(onVolumeUp, onVolumeDown, onNext, onMute, onAllButtons);
    
    int savedVolume = preferences.getInt("Vol", 50);
    if(savedVolume < 10) savedVolume = 10;
    audioPlayer.setVolume(savedVolume);
    strcpy(buf, defaultBtName);
    String deviceName = preferences.getString("btdev", buf);
    if (deviceName.length() > 0) {
      deviceState = STATE_PLAYING;
      audioPlayer.init(deviceName.c_str());
      Serial.println(String("Playing pink noise on ") + deviceName);
    }
    else {
        Serial.println("No device selected");
        deviceState = STATE_SCAN_START;
    }
    delay(100);
    digitalWrite(LED_PIN, 0);
}

void loop() {
    buttonHandler.update();
    handleDeviceState();
    
    // if button is pressed and playing sound, should frequently check button status
    if (buttonHandler.isActive() && deviceState == STATE_PLAYING) {
        delay(10);
    } else {
        delay(100);
    }
}
