#include <Arduino.h>
#include "settings.h"
#include "wifi_comm.h"
#include "hardware.h"
#include "ble.h"

Hardware hardware;
Settings settings;

void setup() {
  // Avvia la seriale di debug integrata nel core
  Serial.begin(115200);

  settings.setup();

  // 1. Inizializziamo il BMS hardware (INA226, Dallas, PIR)
  hardware.setup(&settings);
  hardware.led->startBlink(.25, 100, 1900);

  // 2. Avviamo la tua infrastruttura Wi-Fi, il Server Web e l'aggiornamento via rete
  wifiComm.setup(settings, &hardware);
  BTHomeBeacon_setup(settings.getHostname());
}

void loop() {
  unsigned long now = millis();
  wifiComm.run();
  BTHomeBeacon_run(hardware, now);
  hardware.run(now);
  // TODO intercept restart requests and call:
  // ESP.restart();
}
