#include "AlpacaSwitch.h"
#include "AlpacaCommon.h"
#include <uri/UriBraces.h>

#include "include_config.h"
#include "board.h"

static const int SWITCH_DEVICE_NUMBER         = 0;

static AlpacaDeviceInfo g_SwitchInfo = {
  "Battery",
  "Battery state and controls.",
  "ESP32 Alpaca Switch Driver",
  "1.0",
  2 // ISwitchV2
};

#define BATTERY_VOLTAGE          0
#define BATTERY_CURRENT          1
#define BATTERY_SOC              2
#define BATTERY_TEMPERATURE      3
#define BATTERY_MIN_TEMP         4
#define BATTERY_AUTONOMY         5

#define BATT_SWITCHES            6

#define LIGHT_BRIGHTNESS         0
#define LIGHT_AUTO_ENABLED       1
#define LIGHT_AUTO_BRIGHTNESS    2
#define LIGHT_AUTO_DURATION      3

#define LIGHT_SWITCHES           4

#define OUTLET_1                 0
#define OUTLET_2                 1
#define OUTLET_3                 2

#define MAX_SWITCHES            (BATT_SWITCHES + LIGHT_SWITCHES + MAX_OUTLETS)

enum class SwitchType {
  BatteryVoltage,
  BatteryCurrent,
  BatterySoC,
  BatteryTemperature,
  BatteryMinTemperature,
  BatteryAutonomy,
  LightBrightness,
  LightAutoEnabled,
  LightAutoBrightness,
  LightAutoDuration,
  Outlet1,
  Outlet2,
  Outlet3,
  Unknown
};

static SwitchDef g_switches[MAX_SWITCHES] = {
  { "Voltage",     "Battery voltage (V)",        0.0,  20.0, 0.01, false },
  { "Current", "Battery current (A): negative = discharge, positive = charge", -20.0, 20.0, 0.01, false },  
  { "SoC",         "Battery state of charge (%)",           0.0, 100.0, 1.0,  false },
  { "Temperature", "Battery temperature (\xC2\xB0" "C)", -40.0, 85.0, 0.1,  false },
  { "Min temperature", "Minimum battery temperature (\xC2\xB0" "C)", CP_LOW_THRESHOLD_MIN_C, CP_LOW_THRESHOLD_MAX_C, 1.0, true},
  { "Autonomy", "Estimated remaining autonomy (h), capped at 24: 24 = charging or negligible load", 0.0, 24.0, 0.1, false }  
};

static SwitchDef g_lightSwitches[4] = {
  { "Brightness", "Light manual brightness (%)", 0.0, 100.0, 1.0, true },
  { "Automation", "Enable motion detection based light automation", 0.0, 1.0, 1.0,  true },
  { "Auto brightness", "Light brightness when motion activated (%)", LIGHT_AUTO_BRIGHTNESS_MIN_PCT, 100.0, 1.0,  true },
  { "Auto duration", "Light on duration after no-more motion detected (s)", LIGHT_AUTO_DURATION_MIN_S, LIGHT_AUTO_DURATION_MAX_S, 1.0, true }
};

static SwitchDef g_outletSwitches[MAX_OUTLETS] = {
  { "Power outlet 1", "Power outlet 1 (%)", 0.0, 100.0, 1.0, true },
  { "Power outlet 2", "Power outlet 2 (%)", 0.0, 100.0, 1.0, true },
  { "Power outlet 3", "Power outlet 3 (%)", 0.0, 100.0, 1.0, true }    
};

static DeviceDef g_device;
static bool g_switchConnected;
static int g_numSwitches;

static void buildDeviceTable() {
  g_numSwitches = BATT_SWITCHES;

  if (HAS_LIGHT) {
    for (int i = 0; i < LIGHT_SWITCHES; i++) {
      g_switches[g_numSwitches++] = g_lightSwitches[i];
    }
  }

  if (OUTLET_COUNT > 0) {
    for (int i = 0; i < OUTLET_COUNT; i++) {
      g_switches[g_numSwitches++] = g_outletSwitches[i];
    }
  }
  g_device = { SWITCH_DEVICE_NUMBER, &g_SwitchInfo, g_switches, g_numSwitches };
}

static AlpacaDeviceRef swResolver(int number) {
  if (number != 0) return {};
  return AlpacaDeviceRef(&g_SwitchInfo, &g_switchConnected);
}

static SwitchType resolveSwitchId(int id) {
  if (id < BATT_SWITCHES) {
    switch(id) {
      case BATTERY_VOLTAGE:
        return SwitchType::BatteryVoltage;
      case BATTERY_CURRENT:
        return SwitchType::BatteryCurrent;
      case BATTERY_SOC:
        return SwitchType::BatterySoC;
      case BATTERY_TEMPERATURE:
        return SwitchType::BatteryTemperature;
      case BATTERY_MIN_TEMP:
        return SwitchType::BatteryMinTemperature;
      case BATTERY_AUTONOMY:
        return SwitchType::BatteryAutonomy;
    }
  } else if (HAS_LIGHT && id < BATT_SWITCHES + LIGHT_SWITCHES) {
    switch(id - BATT_SWITCHES) {
      case LIGHT_BRIGHTNESS:
        return SwitchType::LightBrightness;
      case LIGHT_AUTO_ENABLED:
        return SwitchType::LightAutoEnabled;
      case LIGHT_AUTO_BRIGHTNESS:
        return SwitchType::LightAutoBrightness;
      case LIGHT_AUTO_DURATION:
        return SwitchType::LightAutoDuration;
    }
  } else {
    int prev_switches = BATT_SWITCHES + (HAS_LIGHT ? LIGHT_SWITCHES : 0);
    if (id >= prev_switches && id < prev_switches + OUTLET_COUNT) {
      switch(id - prev_switches) {
        case OUTLET_1:
          return SwitchType::Outlet1;
        case OUTLET_2:
          return SwitchType::Outlet2;
        case OUTLET_3:
          return SwitchType::Outlet3;
      }
    }
  }

  return SwitchType::Unknown;
}

DeviceDef getSwitchDevice()   { return g_device; }
int getSwitchDevicesCount()     { return 1; }
bool getSwitchConnected() { return g_switchConnected; }

/*
static bool isValidSwitchId(int id) {
  return id >= 0 && id < g_numSwitches;
}
*/

struct AlpacaSwitchRequest {
  int switchId;
  SwitchType switchType;
  uint32_t ctid;
};

static bool checkRequest(WebServer &server, AlpacaSwitchRequest &request) {
    int dn = AlpacaHelper::pathArgToInt(server, 0, -1);
    int id = AlpacaHelper::queryArgToInt(server, "Id", -1);
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    if (dn != 0) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Device number out of range", ctid);
      return false;
    }

    SwitchType st = resolveSwitchId(id);
    if (st == SwitchType::Unknown) {
    /*if (!isValidSwitchId(id)) { */
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Switch Id out of range", ctid);
      return false;
    }
    
    request.ctid = ctid;
    request.switchId = id;
    request.switchType = st;
    return true;
}

// Voltage/Current: nessuna lettura valida se l'INA226 non risponde.
// Temperature: nessuna lettura valida se il DS18B20 non risponde.
// SoC invece resta esposto anche a sensore guasto: e' un valore accumulato
// e persistito, l'ultima stima nota, non una lettura istantanea.
static bool checkValueSet(WebServer &server, Hardware *hw, const AlpacaSwitchRequest &r) {
  /*
  if (r.switchType == SwitchType::BatteryTemperature && !hw->heater->isTemperatureValid()) {
    AlpacaHelper::sendError(server, AlpacaError::ValueNotSet,
                              "Temperature sensor not available", r.ctid);
    return false;
  }
  if ((r.switchType == SwitchType::BatteryVoltage || r.switchType == SwitchType::BatteryCurrent ) &&
      !hw->battery->isSensorValid()) {
    AlpacaHelper::sendError(server, AlpacaError::ValueNotSet,
                              "Battery sensor (INA226) not available", r.ctid);
    return false;
  }*/
  return true;
}

double getSwitchValue(Hardware *hw, SwitchType type) {
  switch (type) {
    case SwitchType::BatteryVoltage:            return hw->battery->getVoltage();
    case SwitchType::BatteryCurrent:            return hw->battery->getCurrent();
    case SwitchType::BatterySoC:                return hw->battery->getSoC();
    case SwitchType::BatteryTemperature:        return hw->heater->getTemperature();
    case SwitchType::BatteryMinTemperature:     return hw->heater->getLowThreshold();
    case SwitchType::BatteryAutonomy:           return hw->battery->getAutonomyHours();
    case SwitchType::LightBrightness:           return hw->light->getBrightness() * 100.0;
    case SwitchType::LightAutoEnabled:          return hw->light->isAutoEnabled();
    case SwitchType::LightAutoBrightness:       return hw->light->getAutoBrightness() * 100.0;
    case SwitchType::LightAutoDuration:         return hw->light->getAutoDuration();
    case SwitchType::Outlet1:                   return hw->outlets[0]->getPower() * 100.0;
    case SwitchType::Outlet2:                   return hw->outlets[1]->getPower() * 100.0;
    case SwitchType::Outlet3:                   return hw->outlets[2]->getPower() * 100.0;
  }
  return 0.0; //Unknown
}

void writeSwitchValue(Hardware *hw, SwitchType type, double v) {
  switch (type) {
    case SwitchType::BatteryMinTemperature: hw->heater->setLowThreshold(v); break;

    case SwitchType::LightBrightness:       hw->light->setBrightness(v / 100.0f); break;
    case SwitchType::LightAutoEnabled:      hw->light->autoEnable(v != 0.0); break;
    case SwitchType::LightAutoBrightness:   hw->light->setAutoBrightness(v / 100.0f); break;
    case SwitchType::LightAutoDuration:     hw->light->setAutoDuration((int) v); break;

    case SwitchType::Outlet1:               hw->outlets[0]->setPower(v / 100.0f); break;
    case SwitchType::Outlet2:               hw->outlets[1]->setPower(v / 100.0f); break;
    case SwitchType::Outlet3:               hw->outlets[2]->setPower(v / 100.0f); break;
  }
}

void alpacaSwitchSetup(WebServer &server, Hardware *hardware) {
  buildDeviceTable();
  g_switchConnected = true;
  registerCommonDeviceEndpoints(server, "switch", swResolver);

  const String base = "/api/v1/switch/{}/";

  // ------------------ STATIC DATA --------------------

  // maxswitch: numero di switch gestiti da questo device
  server.on(UriBraces(base + "maxswitch"), HTTP_GET, [&server]() {
    int dn = AlpacaHelper::pathArgToInt(server, 0);
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    if (dn != 0) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Number fuori range", ctid);
      return;
    }
    AlpacaHelper::sendInt(server, g_device.num_switches, ctid);
  });

  // getswitchdescription(Id)
  server.on(UriBraces(base + "getswitchdescription"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendString(server, g_device.switches[r.switchId].description, r.ctid);
  });

  // getswitchname(Id)
  server.on(UriBraces(base + "getswitchname"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendString(server, g_device.switches[r.switchId].name, r.ctid);
  });

  // canwrite(Id)
  server.on(UriBraces(base + "canwrite"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendBool(server, g_device.switches[r.switchId].canWrite, r.ctid);
  });
  
// minswitchvalue(Id)
  server.on(UriBraces(base + "minswitchvalue"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, g_device.switches[r.switchId].minValue, r.ctid);
  });

  // maxswitchvalue(Id)
  server.on(UriBraces(base + "maxswitchvalue"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, g_device.switches[r.switchId].maxValue, r.ctid);
  });

  // switchstep(Id)
  server.on(UriBraces(base + "switchstep"), HTTP_GET, [&server]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    AlpacaHelper::sendDouble(server, g_device.switches[r.switchId].step, r.ctid);
  });


  // ------------------ DYNAMIC DATA --------------------
  // boolean switch value
  server.on(UriBraces(base + "getswitch"), HTTP_GET, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    if (!checkValueSet(server, hardware, r)) return;
    const SwitchDef &s = g_device.switches[r.switchId];
    double v = getSwitchValue(hardware, r.switchType);
    AlpacaHelper::sendBool(server, v > (s.minValue + s.maxValue) / 2.0, r.ctid);
  });

  // getswitchvalue(Id) -> il valore analogico vero e proprio (V, A, %, C)
  server.on(UriBraces(base + "getswitchvalue"), HTTP_GET, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    if (!checkValueSet(server, hardware, r)) return;
    AlpacaHelper::sendDouble(server, getSwitchValue(hardware, r.switchType), r.ctid);
  });

  // setswitch(Id, State) -> equivale a setswitchvalue(Max) / setswitchvalue(Min)
  server.on(UriBraces(base + "setswitch"), HTTP_PUT, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (!checkRequest(server, r)) return;
    const SwitchDef &s = g_device.switches[r.switchId];
    if (!s.canWrite) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidOperation,
                              "Switch read-only (sensore)", r.ctid);
      return;
    }
    if (!server.hasArg("State")) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Parametro State mancante", r.ctid);
      return;
    }
    bool state = AlpacaHelper::queryArgToBool(server, "State", false);
    writeSwitchValue(hardware, r.switchType, state ? s.maxValue : s.minValue);
    AlpacaHelper::sendEmptyOk(server, r.ctid);
  });

  // setswitchvalue(Id, Value) -> valore analogico, validato contro min/max
  server.on(UriBraces(base + "setswitchvalue"), HTTP_PUT, [&server, hardware]() {
    AlpacaSwitchRequest r;
    if (! checkRequest(server, r)) return;
    const SwitchDef &s = g_device.switches[r.switchId];
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
    writeSwitchValue(hardware, r.switchType, value);
    AlpacaHelper::sendEmptyOk(server, r.ctid);
  });

  server.on(UriBraces(base + "setswitchname"), HTTP_PUT, [&server]() {
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    AlpacaHelper::sendError(server, AlpacaError::InvalidOperation,
                             "Rinomina non supportata", ctid);
  });
}
