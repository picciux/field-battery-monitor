// IHardwareChangeListener.h
#pragma once

// Un valore per ogni aggregato di dati pubblicamente osservabile, non per singola
// proprietà — Battery ne ha due, con cadenze diverse (dati principali vs autonomia);
// gli altri oggetti hardware, per ora, uno solo ciascuno.
enum class HardwareEvent {
    BatteryMainData,   // Voltage, Current, SoC, Temperature, stato riscaldatore
    BatteryAutonomy,   // AutonomyRemaining — ricalcolata/pushata ogni 30s
    Heater,
    Light,
    Outlet
};

class IHardwareChangeListener {
public:
    virtual ~IHardwareChangeListener() = default;
    virtual void onHardwareChanged(HardwareEvent event, int index) = 0;
};