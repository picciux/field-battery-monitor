
#ifndef WIFI_COMM_H
#define WIFI_COMM_H

#include <WiFi.h>
#include <WebSocketsServer.h>

#include "settings.h"
#include "hardware.h"

class WifiComm {
  public:
    void networkDisconnected();
    void getSettings(Settings &settings);
    void updateSettings(Settings &settings);
    bool sendFile(String path);
    void websocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght, Hardware &hardware);
    void run();
    void setup(Settings &settings, Hardware &hardware);
  protected:
    int readByte();
    void sendByte(char b);
    void broadcastEvent(Hardware &hw);
  private:
    bool searchAndConnectNet(char *ssid, char *pass);
    boolean wifiStart(Settings &s);
    void sendSettings(Settings &s);
    int printStatus(Hardware &hardware, char *buf, int bufsize);
    int printCaps(char *buf, int bufsize);
};

#endif //WIFI_COMM_H
