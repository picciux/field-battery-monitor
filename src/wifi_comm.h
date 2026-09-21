
#ifndef WIFI_COMM_H
#define WIFI_COMM_H

#include <WiFi.h>
#include <WebSocketsServer.h>

#include "settings.h"
#include "hardware.h"
#include "ihardware_change_listener.h"

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
  protected:
    int readByte();
    void sendByte(char b);
    //void broadcastEvent(Hardware &hw);
  private:
    Hardware *hardware;
    bool searchAndConnectNet(char *ssid, char *pass);
    boolean wifiStart(Settings &s);
    void sendSettings(Settings &s);
    int formatEvent(HardwareEvent event, int index, char *buf, size_t size);
    void sendInitialState(uint8_t num);
    int printCaps(char *buf, int bufsize);
};

extern WifiComm wifiComm;

#endif //WIFI_COMM_H
