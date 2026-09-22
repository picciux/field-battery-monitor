
/* 

 */

/***************************** Battery config ********************************/
 
/* Required */
 #define BATTERY_CAPACITY                50 //Ah

//#define SOC_PERSIST_MAX_TIME      10 // minutes
//#define SOC_PERSIST_MAX_DIFF       2 // %

/* SoC automatically resets to 100% when following coditions are all met:
    - voltage is >= SOC_RESET_VOLTAGE
    - charge current is <= (BATTERY_CAPACITY * SOC_RESET_TAIL_CURRENT)
    - above conditions are continuosly met for minimum SOC_RESET_TIME ms. */ 
//#define SOC_RESET_VOLTAGE           14.1 // volt
//#define SOC_RESET_TAIL_CURRENT      0.02 // 2% capacity
//#define SOC_RESET_TIME              ( 3 * 60000 ) // ms: 3 minutes

/* Currents below SOC_MIN_CURRENT_INTEGRATION will not be accounted for. */
//#define SOC_MIN_CURRENT_INTEGRATION 0.02 // A

//#define HEATER_HYSTERESIS_C              4 //C
//#define SAFETY_SOC_LOW                  15    // %
//#define SAFETY_SOC_RECOVER              20    // %
//#define SAFETY_CRITICAL_VOLTAGE_V       11.0  // V
//#define SAFETY_CRITICAL_VOLTAGE_HYST_V   0.3  // V

//#define INA226_ADDR               0x40
//#define INA226_RESISTOR           0.002 // Ohm
//#define INA226_RANGE              20.0   // Ampere

/**************************** Hardware config ********************************/

// Uncomment to disable light and motion-driven automation
//#define DISABLE_LIGHT

// Uncomment for 4-MOSFET boards.
//#define CHANNELS_4


/************************** WiFi network configs *****************************
 * All values REQUIRED. 
 *****************************************************************************/

/* The hostname the board will present as on the network. */
#define DEFAULT_HOSTNAME               "battery-monitor"
#define DEFAULT_DISPLAY_NAME           "Battery Monitor"

/* The PSK to connect to the board when in stand-alone Access Point mode. */
#define DEFAULT_AP_PSK                 "Battery-Monitor"

/* SSID and PSK of preferential network to connect to. */
#define DEFAULT_MAIN_SSID              "myMainWiFiSSID"
#define DEFAULT_MAIN_PSK               "myMainWiFiPassword"

/* SSID and PSK of alternate network to connect to. Will be used when main
   preferential network is not in range. */
#define DEFAULT_ALT_SSID               "myAlternateWiFiSSID"
#define DEFAULT_ALT_PSK                "myAlternateWiFiPassword"

#define DEFAULT_AP_DONT_BE_DEF_GW      1

/************************ Cold protection defaults ***************************
 * All values REQUIRED. 
 *****************************************************************************/
#define DEFAULT_COLD_PROTECTION_LOW_THRESHOLD   5 // °C

/*************** Automatic motion detection light defaults *******************
 * All values REQUIRED. 
 *****************************************************************************/
#define DEFAULT_AUTO_LIGHT_ENABLED              false
#define DEFAULT_AUTO_LIGHT_BRIGHTNESS           0.25f // 0.0f - 1.0f
#define DEFAULT_AUTO_LIGHT_DURATION             30 // seconds


/************************** Debug output configs *****************************
 * For debugging purposes.
 *****************************************************************************/

/* Uncomment to enable debugging over serial in wifi-mode: ignored if wifi 
 * is not enabled. */
//#define WIFI_DEBUG_ON_SERIAL

/* Uncomment to enable debugging over wifi in wifi-mode: ignored if wifi is 
 * not enabled. */
//#define WIFI_DEBUG_ON_WIFI

/* Choose UDP port the debug messages will be broatcasted to. REQUIRED if
 * WIFI_DEBUG_ON_WIFI is enabled. */
//#define WIFI_DEBUG_WIFI_UDP_PORT 1010
