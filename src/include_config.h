
#pragma once

#define VERSION "1.0"

// Defaults, can be overriden by config.h
#define INA226_ADDR           0x40
#define INA226_RESISTOR       0.002 // Ohm
#define INA226_RANGE         20.0   // Ampere

#define SOC_PERSIST_MAX_TIME     10 // minutes
#define SOC_PERSIST_MAX_DIFF      2 // %

#define SOC_RESET_VOLTAGE           14.1 // volt
#define SOC_RESET_TAIL_CURRENT      0.02 // 2% capacity
#define SOC_RESET_TIME              ( 3 * 60000 ) // ms: 3 minutes
#define SOC_MIN_CURRENT_INTEGRATION 0.02 // A

#define HEATER_HYSTERESIS_C              4 //C

#define SAFETY_SOC_LOW                  15    // %: sotto, IsSafe = false
#define SAFETY_SOC_RECOVER              20    // %: sopra, IsSafe torna true (isteresi)
#define SAFETY_CRITICAL_VOLTAGE_V       11.0  // V: sotto, IsSafe = false a prescindere dal SoC
#define SAFETY_CRITICAL_VOLTAGE_HYST_V  0.3   // V: margine di isteresi sul criterio tensione

#define LIGHT_DEFAULT_TRANSITION_MS   1500    // default transition

#include "config.h"

// Scegli UNA sola destinazione per il log di debug (o nessuna, il default).
// DBG/DBGLN sono stile Print (un solo argomento, come Serial.print),
// DBGF e' stile printf. Con nessun backend attivo, tutte e tre sono no-op:
// restano nel codice sorgente senza alcun costo in una build di produzione.
#if defined(DEBUG_ON_SERIAL)
  #define DBG(t)     (Serial.print(t))
  #define DBGLN(t)   (Serial.println(t))
  #define DBGF(...)  (Serial.printf(__VA_ARGS__))
#elif defined(DEBUG_ON_WS)
  #include "ws_debug.h"
  #define DBG(t)     (wsDebugPrint(t))
  #define DBGLN(t)   (wsDebugPrintln(t))
  #define DBGF(...)  (wsDebugf(__VA_ARGS__))
#else
  #define DBG(t)
  #define DBGLN(t)
  #define DBGF(...)
#endif


