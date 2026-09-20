
#pragma once

#include "ihardware_change_listener.h"

namespace BatteryAutonomy {

constexpr float MIN_DISCHARGE_A = 0.05f;   // sotto questa soglia, "N/A" convenzionale
constexpr float AUTONOMY_CAP_H  = 24.0f;   // tetto convenzionale (24h = non significativo)

// avgCurrentA: media a 10 minuti della corrente istantanea, con la convenzione
// di progetto (negativo = scarica, positivo = carica).
// capacityRemainingAh: da Battery::getRemainingAh() (SoC% * capacità nominale).
// Ritorna sempre un valore valido, clampato: il cap stesso è il segnale
// convenzionale di "in carica o consumo trascurabile".
inline float computeHours(float capacityRemainingAh, float avgCurrentA) {
    float dischargeA = -avgCurrentA;
    if (dischargeA < MIN_DISCHARGE_A) return AUTONOMY_CAP_H;
    float hours = capacityRemainingAh / dischargeA;
    return hours > AUTONOMY_CAP_H ? AUTONOMY_CAP_H : hours;
}

} // namespace BatteryAutonomy

class Battery {
    private:
        float soc;
        float capacity;
        float voltage;
        float current;
        float autonomyH = BatteryAutonomy::AUTONOMY_CAP_H;
        unsigned long _last_update;
        unsigned long _last_autonomy;

        IHardwareChangeListener *_listener;
    public:
        float getSoC();
        float getCapacity();
        float getVoltage();
        /* Convention: Negative current -> battery discharge */
        float getCurrent();
        float getRemainingAh();
        float getAutonomyHours();

        void setChangeListener(IHardwareChangeListener* listener);
        void setup(float capacity);
        void run(unsigned long now);
        /* Reset SoC to full 100%. */
        void reset();
};
