#pragma once
#include <Arduino.h>

#define BUTTON_LONG_PRESS_MS  10000

class Button {
  private:
    int pin = -1;
    unsigned long since = 0;
    bool fired = false;
  public:
    void setup(int p) { pin = p; pinMode(pin, INPUT_PULLUP); }

    // true UNA volta quando il tasto resta premuto per almeno 'ms'.
    bool longPress(unsigned long now, unsigned long ms = BUTTON_LONG_PRESS_MS) {
      if (digitalRead(pin) != LOW) { since = 0; fired = false; return false; }
      if (since == 0) { since = now ? now : 1; return false; }
      if (!fired && (now - since) >= ms) { fired = true; return true; }
      return false;
    }
};