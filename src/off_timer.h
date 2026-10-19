// off_timer.h
#pragma once

#include "include_config.h"
#include "clamp_t.h"

class OffTimer {
  bool armed = false;
  unsigned long start = 0, dur = 0;
public:
  void arm(unsigned long now, unsigned long ms) { start = now; dur = clampT(ms, 0ul, 1000ul * OFF_TIMER_MAX_DELAY_S); armed = true; }
  void cancel() { armed = false; }
  bool isArmed() const { return armed; }
  bool expired(unsigned long now) const { return armed && (now - start) >= dur; }
  unsigned remainingS(unsigned long now) const {
    if (!armed) return 0;
    unsigned long el = now - start;
    return el >= dur ? 0 : (unsigned)((dur - el + 999) / 1000);
  }
};