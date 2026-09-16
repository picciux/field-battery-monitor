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

  //Inizializziamo i settings
  initSettings();
  loadSettings(&settings);

  // 1. Inizializziamo il BMS hardware (INA226, Dallas, PIR)
  hardware.setup();


  // 2. Avviamo la tua infrastruttura Wi-Fi, il Server Web e l'aggiornamento via rete
  wifi.setup(settings, hardware);
}

void loop() {
  wifi.run();
  BTHomeBeacon_run(hardware);
  hardware.run();
}
