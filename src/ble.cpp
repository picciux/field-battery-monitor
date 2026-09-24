#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <string>

#include "include_config.h"
#include "ble.h"

#define BLE_UPDATE_INTERVAL_MS   5000
//#define BLE_ADV_INTERVAL_UNITS   0x0640   // 1000 ms (unita' da 0.625 ms)
#define BLE_ADV_INTERVAL_UNITS   0x0C80   // 2000 ms (unita' da 0.625 ms)

// BTHome v2 object IDs (in ordine crescente, come richiesto dalla spec)
#define BTH_BATTERY_PCT      0x01   // uint8, %
#define BTH_VOLTAGE_MV       0x0C   // uint16, 0.001 V
#define BTH_PROBLEM          0x26   // uint8, bool
#define BTH_SAFETY           0x28   // uint8, bool
#define BTH_COUNT_U16        0x3D   // uint16, hours
#define BTH_TEMPERATURE      0x45   // sint16, 0.1 C
#define BTH_CURRENT_SIGNED   0x5D   // sint16, 0.001 A


static void addU8(std::string &s, uint8_t v)   { s += (char)v; }
static void addU16(std::string &s, uint16_t v) { s += (char)(v & 0xFF); s += (char)(v >> 8); }

void BTHomeBeacon_setup(const char *deviceName) {
  BLEDevice::init(deviceName);
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->setAdvertisementType(ADV_TYPE_NONCONN_IND);
  adv->setMinInterval(BLE_ADV_INTERVAL_UNITS);
  adv->setMaxInterval(BLE_ADV_INTERVAL_UNITS);
}

void BTHomeBeacon_run(Hardware &hw, unsigned long now) {
  static unsigned long last = 0;
  if (now - last < BLE_UPDATE_INTERVAL_MS) return;
  last = now;

  const bool sensorOk = hw.battery->isSensorValid();

  float soc = hw.battery->getSoC();
  if (soc < 0.0f) soc = 0.0f;
  if (soc > 100.0f) soc = 100.0f;

  // Blocco service data BTHome v2: UUID 0xFCD2, device info 0x40
  // (v2, non criptato, non trigger-based), poi gli oggetti.
  std::string sd;
  addU8(sd, 0x16);                       // AD type: Service Data - 16 bit UUID
  addU16(sd, 0xFCD2);
  addU8(sd, 0x40);                       // device info

  addU8(sd, BTH_BATTERY_PCT);
  addU8(sd, (uint8_t) roundf(soc));

  if (sensorOk) {
    addU8(sd, BTH_VOLTAGE_MV);
    addU16(sd, (uint16_t) roundf(hw.battery->getVoltage() * 1000.0f));
  }

  addU8(sd, BTH_PROBLEM);
  addU8(sd, sensorOk ? 0 : 1);

  addU8(sd, BTH_SAFETY);
  addU8(sd, hw.battery->isSafe() ? 1 : 0);

  addU8(sd, BTH_COUNT_U16);
  addU16(sd, (uint16_t) roundf(hw.battery->getAutonomyHours() * 60.0f));

  if (hw.heater->isTemperatureValid()) {
    addU8(sd, BTH_TEMPERATURE);
    addU16(sd, (uint16_t)(int16_t) roundf(hw.heater->getTemperature() * 10.0f));
  }

  if (sensorOk) {
    addU8(sd, BTH_CURRENT_SIGNED);
    addU16(sd, (uint16_t)(int16_t) roundf(hw.battery->getCurrent() * 1000.0f));
  }

  std::string payload;
  addU8(payload, 0x02); addU8(payload, 0x01); addU8(payload, 0x06);   // flags
  addU8(payload, (uint8_t) sd.length());                              // lunghezza AD
  payload += sd;

  BLEAdvertisementData data;
  data.addData(payload);

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->stop();
  adv->setAdvertisementData(data);
  adv->start();
}