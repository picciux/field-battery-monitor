
/* 

 */

/*************************** Battery Total Capacity (Ah) *******************************
 */
#define BATTERY_CAPACITY                50 //Ah

#define HEATER_HYSTERESIS_C              4 //C

//#define INA226_ADDR //defaults to 0x40
//#define INA226_RESISTOR //defaults to 0.002 // Ohm
//#define INA226_RANGE    //defaults to 20.0   // Ampere

// Uncomment to disable light and motion-driven automation
//#define DISABLE_LIGHT

// Uncomment for 4-MOSFET boards.
#define CHANNELS_4


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
#define DEFAULT_COLD_PROTECTION_ENABLED         true
#define DEFAULT_COLT_PROTECTION_LOW_THRESHOLD   5 // °C

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
