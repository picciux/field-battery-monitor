#pragma once
#include <Arduino.h>
#include "include_config.h"

#ifndef STALL_LOG_MS
#define STALL_LOG_MS 500
#endif

// Esegue stmt e, se dura >= STALL_LOG_MS, logga etichetta e durata.
// Con DBGF vuoto (nessun debug compilato) il costo e' solo due millis().
#define TIMED(label, stmt) do {                                            \
    uint32_t _t0 = millis();                                               \
    stmt;                                                                  \
    uint32_t _d = millis() - _t0;                                          \
    if (_d >= STALL_LOG_MS)                                                \
      DBGF("[stall] %s: %lu ms (uptime %lu s)\n", label,                   \
           (unsigned long) _d, (unsigned long) (millis() / 1000UL));       \
  } while (0)