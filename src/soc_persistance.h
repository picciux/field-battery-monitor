
#ifndef SOC_PERSISTANCE_H
#define SOC_PERSISTANCE_H

#include <Preferences.h>

Preferences g_soc;

class SoCPersistance {
    private:
        unsigned long lastWrite = 0;
        float lastSoC = 0;
        unsigned long maxTime = 10 * 60000;
        unsigned int maxVariation = 2; //%
    public:
        void setup(int maxMinutes, int maxVariation) {
            g_soc.begin("soc_persist", false);
            this->maxTime = maxMinutes * 60000ul;
            this->maxVariation = maxVariation;
        };

        void update(float soc, unsigned long now) {
            bool tooOld = ((now - lastWrite) >= maxTime);
            bool tooDiff = (fabs(soc - lastSoC) >= maxVariation);
            if (tooOld || tooDiff) {
                g_soc.putFloat("soc", soc);
                lastWrite = now;
                lastSoC = soc;
            }
        };

        float recover() {
            return g_soc.getFloat("soc", 100.0f);
        };
};

SoCPersistance socPersistance;

#endif