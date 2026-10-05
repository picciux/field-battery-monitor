#pragma once

#include <Preferences.h>

class SoCPersistance {
    private:
        Preferences prefs;
        unsigned long lastWrite = 0;
        float lastSoC = 0;
        unsigned long maxTime = 10 * 60000;
        unsigned int maxVariation = 2; //%

        void _write(float soc, unsigned long now) {
            prefs.putFloat("soc", soc);
            lastWrite = now;
            lastSoC = soc;
        }
    public:
        void setup(int maxMinutes, int maxVariation) {
            prefs.begin("soc_persist", false);
            this->maxTime = maxMinutes * 60000ul;
            this->maxVariation = maxVariation;
        };

        void update(float soc, unsigned long now) {
            bool tooOld = ((now - lastWrite) >= maxTime);
            bool tooDiff = (fabsf(soc - lastSoC) >= maxVariation);
            if (tooOld || tooDiff) 
                _write(soc, now);
        };

        void force(float soc, unsigned long now) {
            _write(soc, now);
        }

        float recover() {
            float v = prefs.getFloat("soc", 100.0f);
            lastSoC = v;
            lastWrite = millis();
            return v;
        };
};
