
#ifndef WIFI_COMM_H
#define WIFI_COMM_H

#include <WiFi.h>
#include <WebSocketsServer.h>

#include "settings.h"
#include "hardware.h"
#include "ihardware_change_listener.h"

enum class RetryState : uint8_t { Idle, Scanning, Connecting };

class WifiComm : public IHardwareChangeListener {
  public:
    void networkDisconnected();
    void getSettings(Settings &settings);
    void updateSettings(Settings &settings);
    bool sendFile(String path);
    void websocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
    void run();
    void setup(Settings &settings, Hardware *hardware);
    void onHardwareChanged(HardwareEvent event, int index);
    void requestRestart();
    void restartMDNS();
  protected:
    int readByte();
    void sendByte(char b);
  private:
    Hardware *hardware;
    bool searchAndConnectNet(char *ssid, char *pass, const char *hostname);
    boolean wifiStart(Settings &s);
    void sendSettings(Settings &s);
    int formatEvent(HardwareEvent event, int index, char *buf, size_t size);
    void sendInitialState(uint8_t num);
    int sendCaps(char *buf, int bufsize);
    unsigned long _lastReconnectCheck = 0;
    unsigned long _lastFullRetry = 0;
    unsigned long _restartRequested = 0;
    bool _apMode = false;
    RetryState _retryState = RetryState::Idle;
    unsigned long _retryStart = 0;
    void apRetryStep(unsigned long now);
    void reconnectCheck(unsigned long now);
};

extern WifiComm wifiComm;

#endif //WIFI_COMM_H
