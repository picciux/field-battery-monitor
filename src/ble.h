#pragma once

#include "hardware.h"

void BTHomeBeacon_setup(const char *deviceName);
void BTHomeBeacon_run(Hardware &hardware, unsigned long now);
