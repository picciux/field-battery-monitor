#pragma once

#include <WebServer.h>
#include "hardware.h"

// Registra gli endpoint REST ISafetyMonitor su device_number = 0
void alpacaSafetyMonitorSetup(WebServer &server, Hardware *hardware);

