
#pragma once

namespace BatteryAutonomy {

constexpr float MIN_DISCHARGE_A = 0.05f;   // sotto questa soglia, "N/A" convenzionale
constexpr float AUTONOMY_CAP_H  = 24.0f;   // tetto convenzionale (24h = non significativo)

// avgCurrentA: media a 10 minuti della corrente istantanea (positivo = scarica).
// capacityRemainingAh: da Battery::getCapacityRemainingAh() (SoC% * capacità nominale).
// Ritorna sempre un valore valido, clampato — nessun flag di validità separato,
// il cap stesso è il segnale convenzionale di "in carica o consumo trascurabile".
inline float computeHours(float capacityRemainingAh, float avgCurrentA) {
    if (avgCurrentA < MIN_DISCHARGE_A) return AUTONOMY_CAP_H;
    float hours = capacityRemainingAh / avgCurrentA;
    return hours > AUTONOMY_CAP_H ? AUTONOMY_CAP_H : hours;
}

} // namespace BatteryAutonomy


class Battery {
    private:
        float soc;
        float capacity;
        float voltage;
        float current;
        float autonomyH;
        unsigned long _last_update;
        unsigned long _last_autonomy;
    public:
        float getSoC();
        float getCapacity();
        float getVoltage();
        float getCurrent();
        float getRemainingAh();
        float getAutonomyHours();
        void setup(float capacity);
        void run(unsigned long now);
        /* Reset SoC to full 100%. */
        void reset();
};
