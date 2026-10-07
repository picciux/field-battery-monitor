#pragma once

/*****************************************************************************
 * WebSocket protocol (WebSocketsServer, port 81, JSON text frames).
 *
 * CLIENT -> DEVICE: actions
 *   {"action": "<name>", "id": N, ...parameters}
 *   - "id" is optional: a non-zero integer chosen by the client, echoed back
 *     in the matching "result". Without it the action is fire-and-forget.
 *   - Every action is answered with a "result" message (see below), also on
 *     success. Without "id" a successful result carries no useful information
 *     and clients may ignore it; a failed one should be surfaced.
 *   - There are NO silent defaults: a missing or wrongly-typed mandatory
 *     parameter makes the action fail and nothing is applied.
 *   - Out-of-range numeric values are silently clamped by the device to the
 *     limits advertised in "capabilities"; the resulting state is announced by
 *     the corresponding event.
 *
 * DEVICE -> CLIENT: three kinds of messages
 *   1. Typed messages, identified by the "type" field:
 *        "capabilities", "settings", "result"      (sent to ONE client)
 *   2. Events, identified by the "event" field (names defined below).
 *        Broadcast to ALL clients when the state changes, and sent as the
 *        initial state to a client that just connected.
 *   Both fields are never present in the same message.
 *
 * ON CONNECTION the device sends, in this order:
 *   1. "capabilities"
 *   2. the full current state, as the same events that would be broadcast:
 *      battery_update, battery_autonomy_update, safety_update,
 *      cold_protection_update, light_update (only with light),
 *      outlet_update (one per outlet).
 *   This gives the UI one single format to interpret.
 *
 * Heartbeat: the firmware pings every 15 s, expects a pong within 3 s and
 * closes the connection after 2 misses. battery_update is emitted every
 * second, even with faulty sensors, so a client can treat a few seconds of
 * silence as a dead connection.
 *
 * Security: no TLS and no authentication. Passwords travel in clear text in
 * update_settings (known limitation, see TODO).
 *****************************************************************************/


/*****************************************************************************
 * TYPED MESSAGES (device -> client)
 *****************************************************************************/

/* "result": outcome of an action.
     {"type":"result", "id":7, "payload":true|false, "detail":"..."}
   - id      present only if the action carried a non-zero id.
   - payload true = applied, false = rejected.
   - detail  present only if non-empty (always on rejection, see below).
   Exceptions to "result only after the action": "bad_json" is sent without
   id, because the id could not be read.

   Values of "detail" on rejection:
     bad_json              message is not valid JSON (no id).
     unknown_action        "action" missing or not one of the names below.
     <field name>          mandatory parameter missing or of the wrong type:
                           "temperature", "brightness", "enabled", "seconds",
                           "index", "power".
     bad_index             outlet_power: index out of range.
     no_light              light action on a variant built without light.
     already_on_main | busy | ota_running
                           wifi_rescan refusals (see ACTION_WIFI_RESCAN).
     no_payload            update_settings: "payload" missing or not an object
                           (fields must be nested inside it, not flat).
     factory_reset_confirm update_settings: factory_reset without the right
                           confirmation string.
     <field>[,<field>...]  update_settings: comma-separated list of rejected
                           fields (see ACTION_UPDATE_SETTINGS). Valid fields
                           of the same payload have been saved anyway.
*/

/* "capabilities": static description of the device, used by the UI to show
   only what exists and to calibrate controls without duplicating limits.
     {"type":"capabilities","payload":{...}}
   Payload:
     int    channels               MOSFET channels on board (2 or 4).
     bool   light                  light (and PIR automation) present.
     int    outlets                number of power outlets (0-3).
     int    light_auto_br_min_pct  minimum motion-light brightness, in %.
     int    light_auto_dr_min      minimum motion-light duration, seconds.
     int    light_auto_dr_max      maximum motion-light duration, seconds.
     int    cp_lt_min              minimum heater turn-on temperature, °C.
     int    cp_lt_max              maximum heater turn-on temperature, °C.
     uint   fs_size                NOTE: actually the flash chip size in bytes
                                   (debug data), not the filesystem size.
     string fw_ver                 firmware version.
     string reset_reason           cause of the reset that preceded THIS boot:
                                   poweron, ext, sw, panic, int_wdt, task_wdt,
                                   other_wdt, deepsleep, brownout, sdio,
                                   unknown.
     uint   boot_count             persisted boot counter (survives reboots).
     uint   uptime_s               seconds since this boot.
   Reading rule: "sw" is the memory of a commanded restart (OTA, restart,
   factory reset), not necessarily of the episode under investigation. Look
   at boot_count and uptime_s first: if they did not change, no reboot
   happened. A UI can also detect a completed restart by seeing boot_count
   increase.
*/

/* "settings": configuration, sent ONLY to the client that asked
   (get_settings), and always after update_settings, also on partial
   rejection, so the UI resyncs from the single source of truth.
     {"type":"settings","payload":{...}}
   Payload:
     string hostname         network hostname, also AP SSID.
     string display_name     name shown by the UI.
     string main_ssid
     string alt_ssid         empty = alternate network disabled.
     bool   ap_no_def_gw     don't be default gateway for AP clients.
     string version          firmware version.
   Passwords (main_psk, alt_psk, ap_psk) are NEVER returned.
*/


/*****************************************************************************
 * ACTIONS (client -> device)
 *****************************************************************************/

/* resets SoC to 100% */
#define ACTION_BATTERY_SOC_RESET        "battery_soc_reset"

/* sets cold protection low threshold temperature. 
   Pars:
    - float temperature (clamped to cp_lt_min..cp_lt_max)
*/
#define ACTION_CP_SET_LT                "cp_low_threshold"

/* requests the "settings" message. no pars.
   Answered with a "settings" message followed by result:true. */
#define ACTION_GET_SETTINGS             "get_settings"

/* sets light automation turn-on brightness.
   Pars:
    - float brightness (0.0 - 1.0, clamped to light_auto_br_min_pct..100%)
*/
#define ACTION_LIGHT_AUTO_BRIGHTNESS    "light_auto_brightness"

/* sets light motion automation. Disabling it while it is keeping the light
   on turns the light off.
   Pars:
    - bool enabled
*/
#define ACTION_LIGHT_AUTO_ENABLE        "light_auto_enabled"

/* sets light automation after-motion duration
   Pars:
    - int seconds (clamped to light_auto_dr_min..light_auto_dr_max)
*/
#define ACTION_LIGHT_AUTO_DURATION      "light_auto_duration"

/* manually sets light brightness. Cancels a running motion-light cycle.
   Pars:
    - float brightness (0.0 - 1.0)
   Fails with "no_light" on variants without light.
*/
#define ACTION_LIGHT_BRIGHTNESS         "light_brightness"

/* sets power outlet
   Pars:
    - int index (0..outlets-1, else "bad_index")
    - float power (0.0 - 1.0)
*/
#define ACTION_OUTLET_POWER             "outlet_power"

/* requests device restart. no pars. The result is sent first; the reboot
   follows after ~500 ms, so the connection will drop. */
#define ACTION_RESTART                  "restart"

/* changes settings.
   Pars:
    - object payload, with any subset of:
        string hostname       1-30 chars: a-z, 0-9, '-', not at start or end.
                              Takes effect after restart.
        string display_name   1-30 chars.
        string main_ssid      1-32 chars.
        string alt_ssid       0-32 chars; empty disables the alternate network.
        string main_psk       8-63 chars; empty = unchanged.
        string alt_psk        8-63 chars; empty = unchanged.
        string ap_psk         8-63 chars; empty = unchanged.
        bool   ap_no_def_gw
        bool   factory_reset
        string factory_reset_confirm  must be exactly "CONFIRM FACTORY RESET"
   Absent fields are left untouched. PARTIAL SAVE: valid fields are written
   even if others are rejected; result:false then carries in "detail" the
   comma-separated names of the rejected ones.
   Factory reset takes precedence: with factory_reset:true (and the right
   confirmation) all other fields are ignored, the "settings" defaults are
   sent, and the device restarts. It erases only the "settings" NVS
   namespace (not the SoC nor the boot counter).
   Always followed by a "settings" message (except on no_payload and on
   a refused factory reset).
   Settings are NOT applied to the running network until restart.
*/
#define ACTION_UPDATE_SETTINGS          "update_settings"

/* forces an immediate Wi-Fi scan and switches to MAIN (preferred) or ALT if
   visible and not already in use. Works from AP, from STA on ALT, or while
   disconnected. No pars. Result detail on refusal: already_on_main | busy | ota_running.
   The connection to the device may drop if the switch happens. */
#define ACTION_WIFI_RESCAN              "wifi_rescan"

/*****************************************************************************
 * EVENTS (device -> client)
 *****************************************************************************/

/* update battery state event. Emitted every second, also with faulty sensors.
   Pars:
    - float voltage (null if INA226 doesn't work)
    - float current (null if INA226 doesn't work). Negative = discharge,
      positive = charge.
    - float SoC (always present; with a faulty INA226 it stays frozen at the
      last value)
    - float temperature (null if DS18B20 doesn't work)
    - bool battery_sensor_ok (false when INA226 not responding)
*/
#define EVENT_BATTERY   "battery_update"

/* update battery autonomy event. Emitted every 30 s.
   Pars:
    - float hours: 10-minute-average based estimate, capped at 24.0, where
      24.0 means "charging or negligible load", not a real estimate. The
      first values after boot are not significant.
*/
#define EVENT_BATTERY_AUTONOMY   "battery_autonomy_update"

/* update cold protection state event.
   Pars:
    - float lt low threshold temperature 
*/
#define EVENT_CP        "cold_protection_update"

/* send debug content.
   Pars:
    - string msg debug content 
*/
#define EVENT_DEBUG        "debug"

/* update light state event. Not sent on variants without light.
   Emitted when the target changes (no intermediate events during a fade).
   Pars: 
    - float brightness (target: light could reach that with a transition)
    - bool auto enabled/disabled
    - float auto_br brightness
    - int auto_dr duration 
*/
#define EVENT_LIGHT     "light_update"

/* update power outlets state event.
   Pars:
    - int index
    - float power 
*/
#define EVENT_OUTLET   "outlet_update"

/* debug line, broadcast. Only with DEBUG_ON_WS defined; not usable to
   investigate problems of the websocket itself.
   Pars:
    - string msg
   NOTE: the event name is a literal in ws_debug.cpp, not a macro. */

/* update safety state event. Emitted on change only.
   Pars:
    - bool is_safe
*/
#define EVENT_SAFETY    "safety_update"

/* sent when a forced rescan found nothing to switch to, or the switch failed.
   Pars:
    - bool found (always false) */
#define EVENT_WIFI_SCAN                 "wifi_scan_result"
