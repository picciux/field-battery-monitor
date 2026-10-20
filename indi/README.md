# indi-batterymonitor

Driver INDI per il Battery Monitor ESP32. Parla col firmware via **websocket (porta 81)**,
non via Alpaca.

## Mappatura firmware → INDI

| Firmware | INDI |
|---|---|
| `battery_update` (V, I, SoC, T, `battery_sensor_ok`) | `BATTERY_STATE` (+ `POWER_SENSORS` del PowerInterface) |
| `battery_autonomy_update` | `BATTERY_STATE.AUTONOMY` |
| sensori INA226 / DS18B20 | `BATTERY_SENSORS` (Light) |
| `safety_update` | **`SAFETY_STATUS`** (Light, elemento `SAFETY`) |
| `cold_protection_update` / `cp_low_threshold` | `COLD_PROTECTION.LOW_THRESHOLD` |
| `light_update` brightness / `light_brightness` | `LIGHT.BRIGHTNESS` (se il firmware ha la luce) |
| `light_auto*` | `LIGHT_AUTO`, `LIGHT_AUTO_SETTINGS` (se il firmware ha la luce) |
| `outlet_update` / `outlet_state` | PowerInterface, porte DC `PowerChannels` |
| `battery_soc_reset` | `SOC_RESET` (pulsante) |
| `capabilities` (fw_ver, reset_reason, boot_count, uptime_s) | `FIRMWARE_INFO` + warning se `boot_count` sale |

Luce è esposta con proprietà custom, non definite in `PowerInterface`.
Outlet sono esposti come porte **DC** (on/off) in coerenza con il firmware.

### Valori di `SAFETY_STATUS`

| Stato | Condizione |
|---|---|
| `Ok` | `is_safe = true`, INA226 ok, dati freschi (< 10 s) |
| `Busy` | `is_safe = true` ma INA226 guasto o dati vecchi (il firmware congela `IsSafe`) |
| `Alert` | `is_safe = false` |
| `Idle` | link caduto / nessun dato (configurabile: `LINK_LOST_SAFETY_STATE`) |

Nel driver Safety Monitor usare il nome del device: `Battery Monitor`.

## Build

Dipendenze: INDI ≥ 2.1 (con `indipowerinterface.h` e `indijson.hpp`), IXWebSocket (da sorgenti:
github.com/machinezone/IXWebSocket, con `zlib1g-dev`). Il JSON è quello incluso in libindi;
con `-DUSE_SYSTEM_JSONLIB=ON` si usa invece `nlohmann_json` di sistema.

    mkdir build && cd build
    cmake .. && make && sudo make install

## Da verificare contro gli header libindi installati

1. `PowerInterface::initProperties(...)` chiamato in `updateProperties()` invece che in
   `initProperties()`: serve perché il numero di porte dipende dal firmware. Se non funziona,
   usare un numero fisso di porte.
2. Nomi `processNumber/processSwitch/processText` e `saveConfigItems` del PowerInterface.
3. `PowerSensorsNP`, `SENSOR_VOLTAGE`, `SENSOR_CURRENT` in `pushPowerSensors()`
   (disattivabile con `-DBATTERYMON_FEED_POWER_SENSORS=0`).
4. `mirrorPortsToPowerInterface()` è vuota (TODO): senza di essa la scheda Power non riflette
   cambi di luce/outlet fatti fuori da INDI (web UI, PIR).

## Suggerimenti lato firmware (opzionali)

- Azione `get_state` che rimandi lo stato completo: oggi, se un valore inviato viene clampato
  a quello già presente, il firmware non emette l'evento e il client resta disallineato.
- Un id di correlazione nei messaggi `result`.
