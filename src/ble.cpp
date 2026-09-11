#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

#include "include_config.h"
#include "hardware.h"

void BTHomeBeacon_run() {
  static unsigned long last_ble_time = 0;
  if (millis() - last_ble_time < 5000) return; // Trasmette tassativamente solo ogni 5 secondi
  last_ble_time = millis();

    uint8_t soc_out = (uint8_t)current_soc;
  uint16_t volt_out = (uint16_t)(battery_voltage * 1000.0f);
  int16_t curr_out = (int16_t)(ist_current * 1000.0f);

  // Inizializziamo l'oggetto Advertising dell'ESP32
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  std::string payload = "";
  // Flags standard BLE
  payload += (char)0x02; payload += (char)0x01; payload += (char)0x06;
  // Intestazione BTHome v2 (UUID 0xFCD2)
  payload += (char)0x0C; payload += (char)0x16; payload += (char)0xD2; payload += (char)0xFC; payload += (char)0x40;

  // Dati Sensori
  payload += (char)0x01; payload += (char)soc_out; // SoC
  payload += (char)0x0C; payload += (char)(volt_out & 0xFF); payload += (char)((volt_out >> 8) & 0xFF); // Volt
  payload += (char)0x43; payload += (char)(curr_out & 0xFF); payload += (char)((curr_out >> 8) & 0xFF); // Corrente

  BLEAdvertisementData oAdvertisementData;
  oAdvertisementData.addData(payload);

  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->start();
}
