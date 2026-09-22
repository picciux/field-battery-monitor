
/* resets SoC to 100% */
#define ACTION_BATTERY_SOC_RESET        "battery_soc_reset"

/* sets cold protection low threshold temperature. 
   Pars:
    - float temperature 
*/
#define ACTION_CP_SET_LT                "cp_low_threshold"

/* manually sets light brightness.
   Pars:
    - float brightness (0.0 - 1.0)
*/
#define ACTION_LIGHT_BRIGHTNESS         "light_brightness"

/* sets light motion automation.
   Pars:
    - bool enabled
*/
#define ACTION_LIGHT_AUTO_ENABLE        "light_auto_enabled"

/* sets light automation turn-on brightness.
   Pars:
    - float brightness (0.0 - 1.0)
*/
#define ACTION_LIGHT_AUTO_BRIGHTNESS    "light_auto_brightness"

/* sets light automation after-motion duration
   Pars:
    - int seconds
*/
#define ACTION_LIGHT_AUTO_DURATION      "light_auto_duration"

/* sets power outlet
   Pars:
    - int index
    - float power (0.0 - 1.0)
*/
#define ACTION_OUTLET_POWER             "outlet_power"

/* requests device restart. no pars. */
#define ACTION_RESTART                  "restart"

/* update battery state event.
   Pars:
    - float voltage (can be null if sensor desnt't work)
    - float current (can be null if sensor desnt't work)
    - float SoC
    - float temperature (can be null if sensor desnt't work)
*/
#define EVENT_BATTERY   "battery_update"

/* update battery state event.
   Pars:
    - float hours
*/
#define EVENT_BATTERY_AUTONOMY   "battery_autonomy_update"

/* update safety state event.
   Pars:
    - bool is_safe
*/
#define EVENT_SAFETY    "safety_update"

/* update cold protection state event.
   Pars:
    - int low threshold temperature 
*/
#define EVENT_CP        "cold_protection_update"

/* update light state event.
   Pars: 
    - float light brightness
    - bool auto enabled/disabled
    - float auto brightness
    - int auto duration 
*/
#define EVENT_LIGHT     "light_update"

/* update power outlets state event.
   Pars:
    - int index
    - float power 
*/
#define EVENT_OUTLET   "outlet_update"
