#pragma once

#include <WebServer.h>
#include "alpaca/AlpacaCommon.h"
#include "hardware.h"

const AlpacaDeviceInfo& getSwitchDeviceInfo(int number);
const int getSwitchDevicesCount();

// Registra tutti gli endpoint REST ISwitchV2 su device_number = 0
void alpacaSwitchSetup(WebServer &server, Hardware &hardware);


