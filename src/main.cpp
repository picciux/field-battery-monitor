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
  // 1. Mantiene vivi i servizi di rete, WebSocket e richieste HTTP del tuo codice
  wifi.run();
  BTHomeBeacon_run(hardware);
  // 2. Timer non bloccante per leggere i sensori ed emettere il beacon BLE ogni 5 secondi
  static unsigned long last_bms_time = 0;
  if (millis() - last_bms_time >= 1000) {
    last_bms_time = millis();
    hardware.run();
  }
}
