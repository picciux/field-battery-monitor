#include "diag.h"
#include <esp_system.h>

static const char *g_resetReasonStr = "unknown";

static const char* reasonToStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "poweron";
    case ESP_RST_EXT:       return "ext";
    case ESP_RST_SW:        return "sw";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int_wdt";
    case ESP_RST_TASK_WDT:  return "task_wdt";   // <-- quello che sospetti
    case ESP_RST_WDT:       return "other_wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";   // <-- l'altro sospetto
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

void diagSetup() {
  g_resetReasonStr = reasonToStr(esp_reset_reason());
}

const char* getResetReasonStr() { return g_resetReasonStr; }

