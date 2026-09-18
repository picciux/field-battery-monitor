#pragma once

#include <WebServer.h>
#include "alpaca/AlpacaCommon.h"
#include "hardware.h"

struct SwitchDef {
  const char* name;
  const char* description;
  double minValue;
  double maxValue;
  double step;
  bool   canWrite;
};

struct DeviceDef {
  const int number;
  const struct AlpacaDeviceInfo &devInfo;
  const struct SwitchDef *switches;
  const int num_switches;
};


//const AlpacaDeviceInfo& getSwitchDeviceInfo(int number);
const int getSwitchDevicesCount();
DeviceDef *getSwitchDevices();

// Registra tutti gli endpoint REST ISwitchV2 su device_number = 0
void alpacaSwitchSetup(WebServer &server, Hardware *hardware);


