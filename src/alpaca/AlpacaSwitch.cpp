#include "AlpacaSwitch.h"
#include "AlpacaCommon.h"
#include <uri/UriBraces.h>

#include "include_config.h"
#include "board.h"

static const int BATTERY_SWITCH_DEVICE_NUMBER         = 0;
static const int LIGHT_SWITCH_DEVICE_NUMBER           = 1;
static const int OUTLET_SWITCH_DEVICE_NUMBER           = 2;

// ---------------------------------------------------------------------------
// Definizione statica degli switch. Id 0-3: sensori batteria, read-only
// (CanWrite=false). Id 4-5: uscite scrivibili (relay on/off + PWM 0-100).
// ---------------------------------------------------------------------------

static AlpacaDeviceInfo g_batterySwitchInfo = {
  "Battery",
  "Battery state and controls.",
  "ESP32 Alpaca Switch Driver",
  "1.0",
  2 // ISwitchV2
};

static SwitchDef g_batt_switches[] = {
  { "Voltage",     "Battery voltage (V)",        0.0,  20.0, 0.01, false },
  { "Current", "Battery current (A): negative = discharge, positive = charge", -20.0, 20.0, 0.01, false },  
  { "SoC",         "Battery state of charge (%)",           0.0, 100.0, 1.0,  false },
  { "Temperature", "Battery temperature (\xC2\xB0" "C)", -40.0, 85.0, 0.1,  false },
  { "Min temperature", "Minimum battery temperature (\xC2\xB0" "C)", -10.0,   10.0, 1.0,  true  }
};

#define BATTERY_VOLTAGE     0
#define BATTERY_CURRENT     1
#define BATTERY_SOC         2
#define BATTERY_TEMPERATURE 3
#define BATTERY_MIN_TEMP    4

static AlpacaDeviceInfo g_lightSwitchInfo = {
  "Light",
  "Ambient Light controls.",
  "ESP32 Alpaca Switch Driver",
  "1.0",
  2 // ISwitchV2
};

static SwitchDef g_light_switches[] = {
  //{ "ON",               "Turn ON/OFF",               0.0,  1.0, 1.0, true },
  { "Brightness",          "Light manual brightnerr (%)",     0.0,  100.0, 1.0, true },
  { "Automation",          "Enable motion detection based light automation",           0.0, 1.0, 1.0,  true },
  { "Auto brightness",     "Light brightness when motion activated (%)", 1.0, 100.0, 1.0,  true },
  { "Auto duration",       "Light on duration after no-more motion detected (s)",      10.0,   60.0, 1.0,  true  }
};

#define LIGHT_BRIGHTNESS        0
#define LIGHT_AUTO_ENABLED      1
#define LIGHT_AUTO_BRIGHTNESS   2
#define LIGHT_AUTO_DURATION     3

static AlpacaDeviceInfo g_outletsSwitchInfo = {
  "Power outlets",
  "Power outlets controls.",
  "ESP32 Alpaca Switch Driver",
  "1.0",
  2 // ISwitchV2
};

static SwitchDef g_outlet_switches[MAX_OUTLETS] = {
  { "Power outlet 1", "Power outlet 1 (%)", 0.0, 100.0, 1.0, true },
  { "Power outlet 2", "Power outlet 2 (%)", 0.0, 100.0, 1.0, true },
  { "Power outlet 3", "Power outlet 3 (%)", 0.0, 100.0, 1.0, true }
};

#define OUTLET_1      0
#define OUTLET_2      1
#define OUTLET_3      2

static const int MAX_SWITCH_DEVICES = 3;

static DeviceDef g_devices[MAX_SWITCH_DEVICES];
static int  g_devicesCount = 0;
static bool g_switchConnected[MAX_SWITCH_DEVICES];

static void buildDeviceTable() {
  g_devicesCount = 0;

  g_devices[g_devicesCount++] = { BATTERY_SWITCH_DEVICE_NUMBER, &g_batterySwitchInfo,
      g_batt_switches, (int)(sizeof(g_batt_switches) / sizeof(g_batt_switches[0])) };

  if (HAS_LIGHT)
    g_devices[g_devicesCount++] = { LIGHT_SWITCH_DEVICE_NUMBER, &g_lightSwitchInfo,
        g_light_switches, (int)(sizeof(g_light_switches) / sizeof(g_light_switches[0])) };

  if (OUTLET_COUNT > 0)
    g_devices[g_devicesCount++] = { OUTLET_SWITCH_DEVICE_NUMBER, &g_outletsSwitchInfo,
        g_outlet_switches, OUTLET_COUNT };   // solo i primi OUTLET_COUNT switch
}

DeviceDef *getSwitchDevices()   { return g_devices; }
int getSwitchDevicesCount()     { return g_devicesCount; }

static int findSwitchDeviceIndex(int number) {
  for (int i = 0; i < g_devicesCount; i++)
    if (g_devices[i].number == number) return i;
  return -1;
}

static AlpacaDeviceRef switchResolver(int number) {
  int idx = findSwitchDeviceIndex(number);
  if (idx < 0) return {};
  return AlpacaDeviceRef(g_devices[idx].devInfo, &g_switchConnected[idx]);
}

static bool isValidSwitchId(int index, int id) {
  return index >= 0 && id >= 0 && id < g_devices[index].num_switches;
}

struct AlpacaSwitchRequest {
  int deviceNumber;   // numero fisso: serve a getSwitchValue/writeSwitch*
  int deviceIndex;    // posizione in g_devices: serve a g_devices[...]
  int switchId;
  uint32_t ctid;
};

static bool checkRequest(WebServer &server, AlpacaSwitchRequest &request) {
    int dn = AlpacaHelper::pathArgToInt(server, 0, -1);
    int id = AlpacaHelper::queryArgToInt(server, "Id", -1);
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    int idx = findSwitchDeviceIndex(dn);
    if (idx < 0) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Device number out of range", ctid);
      return false;
    }
    if (!isValidSwitchId(idx, id)) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Switch Id out of range", ctid);
      return false;
    }
    request.ctid = ctid;
    request.deviceIndex = idx;
    request.deviceNumber = dn;
    request.switchId = id;
    return true;
}

double getSwitchValue(Hardware *hw, int number, int id) {
  switch (number) {
    case BATTERY_SWITCH_DEVICE_NUMBER:
      switch (id) {
        case BATTERY_VOLTAGE:     return hw->battery->getVoltage();
        case BATTERY_CURRENT:     return hw->battery->getCurrent();
        case BATTERY_SOC:         return hw->battery->getSoC();
        case BATTERY_TEMPERATURE: return hw->heater->getTemperature();
        case BATTERY_MIN_TEMP:    return hw->heater->getLowThreshold();
      }
      return 0.0;

    case LIGHT_SWITCH_DEVICE_NUMBER:
      if (!hw->light) return 0.0;
      switch (id) {
        case LIGHT_BRIGHTNESS:      return hw->light->getBrightness() * 100.0;
        case LIGHT_AUTO_ENABLED:    return hw->light->isAutoEnabled();
        case LIGHT_AUTO_BRIGHTNESS: return hw->light->getAutoBrightness() * 100.0;
        case LIGHT_AUTO_DURATION:   return hw->light->getAutoDuration();
      }
      return 0.0;

    case OUTLET_SWITCH_DEVICE_NUMBER:
      if (id >= 0 && id < hw->getOutletsNum())
        return hw->outlets[id]->getPower() * 100.0;
      return 0.0;
  }
  return 0.0;
}

void writeSwitchBool(Hardware *hw, int number, int id, bool s) {
  switch (number) {
    case BATTERY_SWITCH_DEVICE_NUMBER:
      if (id == BATTERY_MIN_TEMP) hw->heater->setLowThreshold(0.0);   // da correggere
      break;

    case LIGHT_SWITCH_DEVICE_NUMBER:
      if (!hw->light) break;
      switch (id) {
        case LIGHT_BRIGHTNESS:      hw->light->setBrightness(s ? 1.0f : 0.0f); break;
        case LIGHT_AUTO_ENABLED:    hw->light->autoEnable(s); break;
        case LIGHT_AUTO_BRIGHTNESS: hw->light->setAutoBrightness(s ? 1.0f : 0.0f); break;
        case LIGHT_AUTO_DURATION:
          hw->light->setAutoDuration(s ? g_light_switches[LIGHT_AUTO_DURATION].maxValue
                                       : g_light_switches[LIGHT_AUTO_DURATION].minValue);
          break;
      }
      break;

    case OUTLET_SWITCH_DEVICE_NUMBER:
      if (id >= 0 && id < hw->getOutletsNum())
        hw->outlets[id]->setPower(s ? 1.0f : 0.0f);
      break;
  }
}

void writeSwitchValue(Hardware *hw, int number, int id, double v) {
  switch (number) {
    case BATTERY_SWITCH_DEVICE_NUMBER:
      if (id == BATTERY_MIN_TEMP) hw->heater->setLowThreshold(v);
      break;

    case LIGHT_SWITCH_DEVICE_NUMBER:
      if (!hw->light) break;
      switch (id) {
        case LIGHT_BRIGHTNESS:      hw->light->setBrightness(v / 100.0f); break;
        case LIGHT_AUTO_ENABLED:    hw->light->autoEnable(v != 0.0); break;
        case LIGHT_AUTO_BRIGHTNESS: hw->light->setAutoBrightness(v / 100.0f); break;
        case LIGHT_AUTO_DURATION:   hw->light->setAutoDuration((int) v); break;
      }
      break;

    case OUTLET_SWITCH_DEVICE_NUMBER:
      if (id >= 0 && id < hw->getOutletsNum())
        hw->outlets[id]->setPower(v / 100.0f);
      break;
  }
}

void alpacaSwitchSetup(WebServer &server, Hardware *hardware) {
  buildDeviceTable();
  for (int i = 0; i < g_devicesCount; i++) g_switchConnected[i] = true;
  registerCommonDeviceEndpoints(server, "switch", switchResolver);

  const String base = "/api/v1/switch/{}/";

  // ------------------ STATIC DATA --------------------

  // maxswitch: numero di switch gestiti da questo device
  server.on(UriBraces(base + "maxswitch"), HTTP_GET, [&server]() {
    int dn = AlpacaHelper::pathArgToInt(server, 0);
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    int idx = findSwitchDeviceIndex(dn);
    if (idx < 0) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Number fuori range", ctid);
      return;
    }
    AlpacaHelper::sendInt(server, g_devices[idx].num_switches, ctid);
  });

  // getswitchdescription(Id)
  server.on(UriBraces(base + "getswitchdescription"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendString(server, g_devices[r.deviceIndex].switches[r.switchId].description, r.ctid);
  });

  // getswitchname(Id)
  server.on(UriBraces(base + "getswitchname"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendString(server, g_devices[r.deviceIndex].switches[r.switchId].name, r.ctid);
  });

  // canwrite(Id)
  server.on(UriBraces(base + "canwrite"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendBool(server, g_devices[r.deviceIndex].switches[r.switchId].canWrite, r.ctid);
  });
  
// minswitchvalue(Id)
  server.on(UriBraces(base + "minswitchvalue"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, g_devices[r.deviceIndex].switches[r.switchId].minValue, r.ctid);
  });

  // maxswitchvalue(Id)
  server.on(UriBraces(base + "maxswitchvalue"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, g_devices[r.deviceIndex].switches[r.switchId].maxValue, r.ctid);
  });

  // switchstep(Id)
  server.on(UriBraces(base + "switchstep"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, g_devices[r.deviceIndex].switches[r.switchId].step, r.ctid);
  });


  // ------------------ DYNAMIC DATA --------------------
  // boolean switch value
  server.on(UriBraces(base + "getswitch"), HTTP_GET, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendBool(server, getSwitchValue(hardware, r.deviceNumber, r.switchId) != 0.0, r.ctid);
  });

  // getswitchvalue(Id) -> il valore analogico vero e proprio (V, A, %, C)
  server.on(UriBraces(base + "getswitchvalue"), HTTP_GET, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, getSwitchValue(hardware, r.deviceNumber, r.switchId), r.ctid);
  });

  // setswitch(Id, State) -> on/off "grezzo"
  server.on(UriBraces(base + "setswitch"), HTTP_PUT, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    if (!g_devices[r.deviceIndex].switches[r.switchId].canWrite) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidOperation,
                               "Switch read-only (sensore)", r.ctid);
      return;
    }
    bool state = AlpacaHelper::queryArgToBool(server, "State", false);
    writeSwitchBool(hardware, r.deviceNumber, r.switchId, state);
    AlpacaHelper::sendEmptyOk(server, r.ctid);
  });

  // setswitchvalue(Id, Value) -> valore analogico, validato contro min/max
  server.on(UriBraces(base + "setswitchvalue"), HTTP_PUT, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    const SwitchDef s = g_devices[r.deviceIndex].switches[r.switchId];
    if (!s.canWrite) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidOperation,
                               "Switch read-only (sensore)", r.ctid);
      return;
    }
    if (!server.hasArg("Value")) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Parametro Value mancante", r.ctid);
      return;
    }
    double value = AlpacaHelper::queryArgToDouble(server, "Value", 0.0);
    if (value < s.minValue || value > s.maxValue) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue,
                               "Value fuori range [min,max]", r.ctid);
      return;
    }
    writeSwitchValue(hardware, r.deviceNumber, r.switchId, value);
    AlpacaHelper::sendEmptyOk(server, r.ctid);
  });

  server.on(UriBraces(base + "setswitchname"), HTTP_PUT, [&server]() {
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    AlpacaHelper::sendError(server, AlpacaError::InvalidOperation,
                             "Rinomina non supportata", ctid);
  });
}
