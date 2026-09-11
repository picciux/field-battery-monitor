
#ifndef INCLUDE_CONFIG_H
#define INCLUDE_CONFIG_H

#define VERSION "1.0"

// Defaults, can be overriden by config.hs
#define INA226_ADDR           0x40
#define INA226_RESISTOR       0.002 // Ohm
#define INA226_RANGE         20.0   // Ampere

#undef CHANNELS_4
#undef DISABLE_LIGHT

#include "config.h"

#ifdef WIFI_DEBUG_ON_SERIAL
  #define DBG(t) (Serial.print(t))
  #define DBGLN(t) (Serial.println(t))
#elif defined(WIFI_DEBUG_ON_WIFI)
  #include "wifi_debug.h"
  #define DBG(t) (getWifiDebug()->print(t))
  #define DBGLN(t) (getWifiDebug()->println(t))
#else
  #define DBG(t) 
  #define DBGLN(t) 
#endif

#endif //INCLUDE_CONFIG_H
