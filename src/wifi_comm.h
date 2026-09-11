
#ifndef WIFI_COMM_H
#define WIFI_COMM_H

#include <WiFi.h>
#include <WebSocketsServer.h>

#include "settings.h"

class WifiComm {
  public:
    void networkDisconnected();
    void getSettings();
    void updateSettings();
    bool sendFile(String path);
    void websocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
    void run();
    void setup();
  protected:
    int readByte();
    void sendByte(char b);
    void broadcastEvent();
  private:
    bool searchAndConnectNet(char *ssid, char *pass);
    boolean wifiStart(Settings *s);
    void sendSettings(Settings *s);
    int printStatus(char *buf, int bufsize);
    int printCaps(char *buf, int bufsize);
};

#endif //WIFI_COMM_H
