#include <Arduino.h>
#include "settings.h"
#include "wifi_comm.h"
#include "hardware.h"
#include "ble.h"

WifiComm wifi;
Hardware hardware;
Settings settings;

void setup() {
  // Avvia la seriale di debug integrata nel core
  Serial.begin(115200);

  settings.setup();

  // 1. Inizializziamo il BMS hardware (INA226, Dallas, PIR)
  hardware.setup(&settings);

  // 2. Avviamo la tua infrastruttura Wi-Fi, il Server Web e l'aggiornamento via rete
  wifi.setup(settings, hardware);
}

void loop() {
  unsigned long now = millis();
  wifi.run();
  BTHomeBeacon_run(hardware, now);
  hardware.run(now);
  // TODO intercept restart requests and call:
  // ESP.restart();
}
