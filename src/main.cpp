#include <Arduino.h>
#include <esp_task_wdt.h>
#include "settings.h"
#include "wifi_comm.h"
#include "hardware.h"
#include "ble.h"

#define WDT_TIMEOUT_S   30

Hardware hardware;
Settings settings;

void setup() {
  // Avvia la seriale di debug integrata nel core
  Serial.begin(115200);

  settings.setup();

  // 1. Inizializziamo il BMS hardware (INA226, Dallas, PIR)
  hardware.setup(&settings);
  hardware.led->startBlink(.25, 50, 3950);

  // 2. Avviamo la tua infrastruttura Wi-Fi, il Server Web e l'aggiornamento via rete
  wifiComm.setup(settings, &hardware);
  BTHomeBeacon_setup(settings.getHostname());

  esp_task_wdt_init(WDT_TIMEOUT_S, true);   // true = reboot automatico allo scadere
  esp_task_wdt_add(NULL);                   // aggiunge il task corrente (loop)
}

void loop() {
  wifiComm.run();
  unsigned long now = millis();
  BTHomeBeacon_run(hardware, now);
  hardware.run(now);
  esp_task_wdt_reset();
}
