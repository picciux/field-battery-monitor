
#pragma once

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#include "settings.h"
#include "hardware.h"
#include "ihardware_change_listener.h"

enum class RetryState : uint8_t { Idle, Scanning, Connecting };

class WifiComm : public IHardwareChangeListener {
  public:
    void networkDisconnected();
    //void getSettings(Settings &settings);
    bool sendFile(String path);
    void sendResult(uint8_t num, uint32_t id, bool ok, const char *detail = nullptr);
    void websocketEvent(Settings &s, uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
    void run();
    void setup(Settings &settings, Hardware *hardware);
    void onHardwareChanged(HardwareEvent event, int index);
    void requestRestart();
    void restartMDNS();
    bool requestRescan(const char *&detail);
  private:
    Hardware *hardware;
    bool searchAndConnectNet(char *ssid, char *pass, const char *hostname);
    boolean wifiStart(Settings &s);
    void sendSettings(Settings &s, uint8_t num);
    bool updateSettings(Settings &s, uint8_t num, JsonVariantConst payload, char *detail, size_t detailSize);
    int formatEvent(HardwareEvent event, int index, char *buf, size_t size);
    void sendInitialState(uint8_t num);
    int sendCaps(char *buf, int bufsize);
    unsigned long _lastReconnectCheck = 0;
    unsigned long _lastFullRetry = 0;
    unsigned long _restartRequested = 0;
    bool _apMode = false;
    RetryState _retryState = RetryState::Idle;
    unsigned long _retryStart = 0;
    unsigned long _staDownSince = 0;
    bool _forceScan = false;
    bool _pickedMain = false;
    void rescanStep(unsigned long now);
    void startRuntimeAp(unsigned long now);    
    void reconnectCheck(unsigned long now);
};

extern WifiComm wifiComm;
