#pragma once

#if __has_include("config.h")
  #include "config.h"
#else
  #error "Missing src/config.h: copy src/config.example.h to src/config.h and edit it."
#endif

#define VERSION "0.9.7"

/*********** REQUIRED DEFINES CHECK ************/
#ifndef BATTERY_CAPACITY
#error "BATTERY_CAPACITY must be defined in config.h"
#endif

#ifndef DEFAULT_HOSTNAME
#error "DEFAULT_HOSTNAME must be defined in config.h"
#endif

#ifndef DEFAULT_DISPLAY_NAME
#error "DEFAULT_DISPLAY_NAME must be defined in config.h"
#endif

/* The PSK to connect to the board when in stand-alone Access Point mode. */
#ifndef DEFAULT_AP_PSK
#error "DEFAULT_AP_PSK must be defined in config.h"
#endif

/* SSID and PSK of preferential network to connect to. */
#ifndef DEFAULT_MAIN_SSID
#error "DEFAULT_MAIN_SSID must be defined in config.h"
#endif

#ifndef DEFAULT_MAIN_PSK
#error "DEFAULT_MAIN_PSK must be defined in config.h"
#endif

/* SSID and PSK of alternate network to connect to. Will be used when main
   preferential network is not in range. */
#ifndef DEFAULT_ALT_SSID
#define DEFAULT_ALT_SSID               ""
#endif

#ifndef DEFAULT_ALT_PSK
#define DEFAULT_ALT_PSK                ""
#endif

#ifndef DEFAULT_AP_DONT_BE_DEF_GW
#define DEFAULT_AP_DONT_BE_DEF_GW         1
#endif


/*********** DEFAULTS (can be overriden in config.h) ************/

/* ------- INA226 ------*/
#ifndef INA226_ADDR
#define INA226_ADDR           0x40
#endif

#ifndef INA226_RESISTOR
#define INA226_RESISTOR       0.002 // Ohm
#endif

#ifndef INA226_RANGE
#define INA226_RANGE          20.0   // Ampere
#endif

/* ------- SoC ------*/
#ifndef SOC_PERSIST_MAX_TIME
#define SOC_PERSIST_MAX_TIME     10 // minutes
#endif

#ifndef SOC_PERSIST_MAX_DIFF
#define SOC_PERSIST_MAX_DIFF      2 // %
#endif

#ifndef SOC_RESET_VOLTAGE
#define SOC_RESET_VOLTAGE           14.1 // volt
#endif

#ifndef SOC_RESET_TAIL_CURRENT
#define SOC_RESET_TAIL_CURRENT      0.02 // 2% capacity
#endif

#ifndef SOC_RESET_TIME
#define SOC_RESET_TIME              ( 3 * 60000 ) // ms: 3 minutes
#endif

#ifndef SOC_MIN_CURRENT_INTEGRATION
#define SOC_MIN_CURRENT_INTEGRATION 0.02 // A
#endif

/* ------- HEATER ------*/
#ifndef HEATER_HYSTERESIS_C
#define HEATER_HYSTERESIS_C              4 //C
#endif

#ifndef CP_LOW_THRESHOLD_MIN_C
#define CP_LOW_THRESHOLD_MIN_C       -10.0f
#endif

#ifndef CP_LOW_THRESHOLD_MAX_C
#define CP_LOW_THRESHOLD_MAX_C        10.0f
#endif

#ifndef DEFAULT_COLD_PROTECTION_LOW_THRESHOLD
#define DEFAULT_COLD_PROTECTION_LOW_THRESHOLD   5.0f // °C
#endif

/* ------- SAFETY MON ------*/
#ifndef SAFETY_SOC_LOW
#define SAFETY_SOC_LOW                  15    // %: sotto, IsSafe = false
#endif

#ifndef SAFETY_SOC_RECOVER
#define SAFETY_SOC_RECOVER              20    // %: sopra, IsSafe torna true (isteresi)
#endif

#ifndef SAFETY_CRITICAL_VOLTAGE_V
#define SAFETY_CRITICAL_VOLTAGE_V       11.0  // V: sotto, IsSafe = false a prescindere dal SoC
#endif

#ifndef SAFETY_CRITICAL_VOLTAGE_HYST_V
#define SAFETY_CRITICAL_VOLTAGE_HYST_V  0.3   // V: margine di isteresi sul criterio tensione
#endif

/* ------- LIGHT ------*/
#ifndef DEFAULT_AUTO_LIGHT_ENABLED
#define DEFAULT_AUTO_LIGHT_ENABLED              false
#endif

#ifndef DEFAULT_AUTO_LIGHT_BRIGHTNESS
#define DEFAULT_AUTO_LIGHT_BRIGHTNESS           0.25f // 0.0f - 1.0f
#endif

#ifndef DEFAULT_AUTO_LIGHT_DURATION
#define DEFAULT_AUTO_LIGHT_DURATION             30 // seconds
#endif

#ifndef LIGHT_DEFAULT_TRANSITION_MS
#define LIGHT_DEFAULT_TRANSITION_MS   1500    // default transition
#endif

#ifndef LIGHT_AUTO_DURATION_MIN_S
#define LIGHT_AUTO_DURATION_MIN_S     10
#endif

#ifndef LIGHT_AUTO_DURATION_MAX_S
#define LIGHT_AUTO_DURATION_MAX_S     60
#endif

#ifndef LIGHT_AUTO_BRIGHTNESS_MIN_PCT
#define LIGHT_AUTO_BRIGHTNESS_MIN_PCT  1
#endif

#ifndef LIGHT_AUTO_BRIGHTNESS_MIN
#define LIGHT_AUTO_BRIGHTNESS_MIN     (LIGHT_AUTO_BRIGHTNESS_MIN_PCT / 100.0f)
#endif

#ifndef LIGHT_FADE_GAMMA
#define LIGHT_FADE_GAMMA 2.0f
#endif

/* ------- ALPACA ------*/
#ifndef ALPACA_LOCATION
#define ALPACA_LOCATION "Unknown"
#endif

/* ------- DEBUG MACROs ------*/
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


