#include "diag.h"
#include <esp_system.h>
#include <Preferences.h>

static const char *g_resetReasonStr = "unknown";
static uint32_t g_rebootCount = 0;
static Preferences g_diagPrefs;

static const char* reasonToStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "poweron";
    case ESP_RST_EXT:       return "ext";
    case ESP_RST_SW:        return "sw";        // esp_restart()/ESP.restart() chiamato esplicitamente
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int_wdt";
    case ESP_RST_TASK_WDT:  return "task_wdt";
    case ESP_RST_WDT:       return "other_wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

void diagSetup() {
  g_resetReasonStr = reasonToStr(esp_reset_reason());

  // Contatore persistito: sopravvive ai reboot, a differenza dell'uptime
  // (che riparte da zero ad ogni boot). Namespace NVS dedicato, separato da
  // "settings" e "soc_persist" per non essere toccato da factoryReset().
  g_diagPrefs.begin("diag", false);
  g_rebootCount = g_diagPrefs.getUInt("boot_count", 0) + 1;
  g_diagPrefs.putUInt("boot_count", g_rebootCount);
}

const char* getResetReasonStr() { return g_resetReasonStr; }
uint32_t getRebootCount() { return g_rebootCount; }