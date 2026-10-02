
/* Copy this file to config.h (git-ignored) and edit it for your setup.
   Anything left commented out falls back to the defaults in include_config.h.
   Required: BATTERY_CAPACITY and the DEFAULT_* values in Wifi Network 
   sections below marked 'REQUIRED'. */


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

//#define LIGHT_DEFAULT_TRANSITION_MS 1500 // default light transition in ms
//#define LIGHT_FADE_GAMMA 2.0f // gamma correction for light transition

/************************** WiFi network configs *****************************
 * All values REQUIRED. 
 *****************************************************************************/

/* The hostname the board will present as on the network. REQUIRED. */
#define DEFAULT_HOSTNAME               "battery-monitor"
#define DEFAULT_DISPLAY_NAME           "Battery Monitor"

/* The PSK to connect to the board when in stand-alone Access Point mode. REQUIRED. */
#define DEFAULT_AP_PSK                 "Battery-Monitor"

/* SSID and PSK of preferential network to connect to. REQUIRED */
#define DEFAULT_MAIN_SSID              "myMainWiFiSSID"
#define DEFAULT_MAIN_PSK               "myMainWiFiPassword"

/* SSID and PSK of alternate network to connect to. Will be used when main
   preferential network is not in range. */
//#define DEFAULT_ALT_SSID               ""
//#define DEFAULT_ALT_PSK                ""

/* Wheater the board should avoid to present itself as the default gateway 
   to DHCP clients when in AP mode.
   0           -> Present itself as default gateway. 
   1 (default) -> Don't present itself as the default gateway.
*/
//#define DEFAULT_AP_DONT_BE_DEF_GW      1


/************************ Cold protection defaults ***************************
 *****************************************************************************/
//#define DEFAULT_COLD_PROTECTION_LOW_THRESHOLD   5.0f // °C

/*************** Automatic motion detection light defaults *******************
 *****************************************************************************/
//#define DEFAULT_AUTO_LIGHT_ENABLED              false
//#define DEFAULT_AUTO_LIGHT_BRIGHTNESS           0.25f // 0.0f - 1.0f
//#define DEFAULT_AUTO_LIGHT_DURATION             30 // seconds


/********************** Alpaca server customization **************************
 *****************************************************************************/
//#define ALPACA_LOCATION    "Unknown"

/************************** Debug output configs *****************************
 * For debugging purposes.
 *****************************************************************************/

/* Scegli UNA delle opzioni sottostanti per il debug (o nessuna).
   Mutuamente esclusive: solo la prima definita, in ordine, viene usata. */

/* Debug via seriale USB. */
//#define DEBUG_ON_SERIAL

/* Debug via evento websocket "debug", visibile a ogni client connesso
   (utile senza seriale collegata, es. con la sola alimentazione USB o
   quando il device e' remoto). */
//#define DEBUG_ON_WS

