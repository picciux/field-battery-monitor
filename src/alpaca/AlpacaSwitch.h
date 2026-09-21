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
  int number;
  const AlpacaDeviceInfo *devInfo;
  const SwitchDef *switches;
  int num_switches;
};

//const AlpacaDeviceInfo& getSwitchDeviceInfo(int number);
int getSwitchDevicesCount();
DeviceDef *getSwitchDevices();

// Registra tutti gli endpoint REST ISwitchV2 su device_number = 0
void alpacaSwitchSetup(WebServer &server, Hardware *hardware);
