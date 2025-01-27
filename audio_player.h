#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <BluetoothA2DPSource.h>
#include <vector>
#include <string>
#include "pink_noise.h"
#include "config.h"

class AudioPlayer {
private:
    BluetoothA2DPSource* a2dp_source = nullptr;
    static int noiseAlgorithm;
    static const int MaxNoiseAlg = 2;
    static bool isPlaying;
    int btVolume = 50;
    std::vector<std::string> *btDevices = nullptr;

    static AudioPlayer* instance;

    static bool scan_callback(const char* name, esp_bd_addr_t address, int rssi) {
        if (instance) {
            if (std::find(instance->btDevices->begin(), instance->btDevices->end(), std::string(name)) == instance->btDevices->end()) {
                instance->btDevices->push_back(std::string(name));
                Serial.print("Found device: ");
                Serial.println(name);
            }
        }
        return false;
    }

public:
    AudioPlayer() {
        instance = this;
        btDevices = new std::vector<std::string>();
    }

    ~AudioPlayer() {
        delete btDevices;
        if (instance == this) {
            instance = nullptr;
        }
        if (a2dp_source) {
            delete a2dp_source;
            a2dp_source = nullptr;
        }
    }

    void init(const char* btSpeaker) {
        if (!a2dp_source) {
          a2dp_source = new BluetoothA2DPSource();
        }
        a2dp_source->set_volume(btVolume);
        a2dp_source->start(btSpeaker, get_sound_data);
    }

    void stop() {
        Serial.println("AudioPlayer stop");
        if (a2dp_source) {
            a2dp_source->set_connected(false);
            a2dp_source->set_auto_reconnect(false);
            a2dp_source->end(true);
            delay(1000);
            Serial.println("bt disconnected");
            delete a2dp_source;
            a2dp_source = NULL;
        }
    }

    void startScan() {
        Serial.println("AudioPlayer startScan");
        btDevices->clear();
        if (a2dp_source == nullptr) {
            a2dp_source = new BluetoothA2DPSource();
        }
        a2dp_source->set_connected(false);
        Serial.println("AudioPlayer disconnect()");
        for(int i = 0; i<20; i++) {
          digitalWrite(LED_PIN, i % 2);
          delay(500);
        }

        a2dp_source->set_ssid_callback(scan_callback);
        a2dp_source->set_auto_reconnect(false);
        a2dp_source->set_data_callback_in_frames(get_sound_data);
        a2dp_source->set_valid_cod_service(ESP_BT_COD_SRVC_AUDIO);
        a2dp_source->start();
        Serial.println("AudioPlayer start()");
    }

    const std::vector<std::string>* getDevices() const {
        return btDevices;
    }

    void setVolume(int volume) {
        btVolume = volume;
        if(btVolume > 100) btVolume = 100;
        if(btVolume < 0) btVolume = 0;
        if (a2dp_source) {
            a2dp_source->set_volume(btVolume);
        }
    }

    int getVolume() { return btVolume; }
    
    void togglePlay() {
        isPlaying = !isPlaying;
    }

    bool getIsPlaying() { return isPlaying; }

    void nextAlgorithm() {
        noiseAlgorithm++;
        if(noiseAlgorithm > MaxNoiseAlg) noiseAlgorithm = 0;
    }

    int getCurrentAlgorithm() { return noiseAlgorithm; }

    static int32_t get_sound_data(Frame* data, int32_t frameCount) {
        if (!isPlaying) {
            for (int i = 0; i < frameCount; i++) {
                data[i].channel1 = 0;
                data[i].channel2 = 0;
            }
            return frameCount;
        }
        
        for (int i = 0; i < frameCount; i++) {
            float sample;
            switch(noiseAlgorithm) {
                case 0: { 
                    float pinkNoise = pinkNoiseFilterV2.generateSample();
                    sample = pinkNoise;
                }
                break;
                case 1: {
                    float brownNoise = brownNoiseGenerator.generateSample();
                    sample = brownNoise;
                }
                break;
                default: { // cursor
                    sample = generate_pink_noise() * 0.5;
                }
            }
            
            // convert float to 16-bit integer
            int16_t pcm = static_cast<int16_t>(sample * 32767);
            data[i] = Frame(pcm);
        }
        return frameCount;
    }
};

AudioPlayer* AudioPlayer::instance = nullptr;
int AudioPlayer::noiseAlgorithm = 1;
bool AudioPlayer::isPlaying = true;

#endif 