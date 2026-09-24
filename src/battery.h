
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
        bool  _sensorValid = true;
        bool  _socUnsafe = false;
        bool  _voltageUnsafe = false;
        unsigned long _last_update;
        unsigned long _last_autonomy;
        unsigned long _start_soc_reset_condition = 0;
        bool _inaConfigured = false;

        IHardwareChangeListener *_listener;

        void updateSafety();
        bool configureIna();
        
    public:
        float getSoC();
        float getCapacity();
        float getVoltage();
        /* Convention: Negative current -> battery discharge */
        float getCurrent();
        float getRemainingAh();
        float getAutonomyHours();
        bool  isSensorValid() const { return _sensorValid; }

        // Fonte unica di verita' per lo stato di sicurezza: SoC con isteresi
        // 15/20% (SAFETY_SOC_LOW/RECOVER) in OR con un backstop di tensione
        // critica (SAFETY_CRITICAL_VOLTAGE_V), a sua volta con isteresi.
        // Congelato all'ultimo valore noto mentre isSensorValid() e' falso:
        // priorita' all'imaging, la scarica profonda resta comunque protetta
        // dal BMS della batteria. Alpaca SafetyMonitor e l'evento websocket
        // "safety_update" leggono entrambi da qui, nessuno dei due ricalcola.
        bool isSafe() const { return !(_socUnsafe || _voltageUnsafe); }

        void setChangeListener(IHardwareChangeListener* listener);
        void setup(float capacity);
        void run(unsigned long now);
        /* Reset SoC to full 100%. */
        void reset();
};
