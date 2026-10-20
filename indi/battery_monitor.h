#pragma once

#include <defaultdevice.h>
#include <indipowerinterface.h>
#include <indipropertylight.h>
#include <indipropertynumber.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>

#include "batterymon_link.h"

/**
 * Driver INDI per il Battery Monitor ESP32 (LiFePO4 + INA226 + DS18B20).
 * Parla col firmware via websocket (porta 81), non via Alpaca.
 *
 *  - INDI::PowerInterface: "porte" = luce (se presente) + outlet, esposte come
 *    porte a duty-cycle (Dew) perche' il firmware le pilota in PWM 0..100%.
 *    Porta 0 = Light (se HAS_LIGHT), poi Outlet 1..N.
 *  - Dati batteria: proprieta' BATTERY_STATE + sensori (V / I) nel PowerInterface.
 *  - SAFETY_STATUS (Light, elemento SAFETY): letto dal driver INDI "Safety Monitor".
 */
class BatteryMonitor : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        BatteryMonitor();

        const char *getDefaultName() override;

        void ISGetProperties(const char *dev) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool Connect() override;
        bool Disconnect() override;
        void TimerHit() override;
        bool saveConfigItems(FILE *fp) override;

        // ---- INDI::PowerInterface ----
        bool SetPowerPort(size_t port, bool enabled) override;
        bool SetDewPort(size_t port, bool enabled, double dutyCycle) override;
        bool SetVariablePort(size_t port, bool enabled, double voltage) override;
        bool SetLEDEnabled(bool enabled) override;
        bool SetAutoDewEnabled(size_t port, bool enabled) override;
        bool CyclePower() override;
        bool SetUSBPort(size_t port, bool enabled) override;

    private:
        // ---- stato -> proprieta' ----
        void applySnapshot(const bm::State &s, uint32_t dirty);
        IPState evaluateSafety(const bm::State &s) const;
        void pushPowerSensors(const bm::State &s);
        void mirrorPortsToPowerInterface(const bm::State &s);
        void noteBootCount(const bm::Capabilities &c);

        // ---- PowerInterface: inizializzazione differita (le porte dipendono dalla variante firmware) ----
        void setupPowerInterface();
        struct PortTarget { bool isLight; int outlet; };

        bm::TimedOffTarget resolvePort(int switchIndex, int &outletIndex);

        bm::Link link_;

        // Variante hardware, letta dalle capabilities in Connect()
        bm::Capabilities caps_;
        bool hasLight_ = false;
        int outlets_ = 0;
        bool powerInitDone_ = false;
        int powerInitPorts_ = -1;

        bool linkWasUp_ = false;
        uint32_t lastBootCount_ = 0;
        std::string lastLoggedError_;

        // ---- proprieta' ----
        INDI::PropertyText ServerAddressTP {2};
        enum { ADDR_HOST, ADDR_PORT };

        INDI::PropertyNumber BatteryNP {5};
        enum { B_VOLTAGE, B_CURRENT, B_SOC, B_TEMPERATURE, B_AUTONOMY };

        INDI::PropertyLight SensorsLP {2};
        enum { S_CURRENT_VOLTAGE, S_TEMPERATURE };

        INDI::PropertyLight SafetyStatusLP {1};   // standard: SAFETY_STATUS / SAFETY

        INDI::PropertyNumber SocResetVoltageNP {1};
        INDI::PropertyNumber ColdProtectionNP {1};

        INDI::PropertyNumber LightBrigthnessNP{1};
        INDI::PropertySwitch LightAutoSP {2};
        enum { AUTO_ON, AUTO_OFF };
        INDI::PropertyNumber LightAutoNP {2};
        enum { LA_BRIGHTNESS, LA_DURATION };

        INDI::PropertySwitch SocResetSP {1};

        INDI::PropertyText SystemInfoTP {5};
        enum { SYS_NAME, FW_VERSION, SYS_RESET_REASON, SYS_BOOT_COUNT, SYS_UPTIME };

        INDI::PropertyNumber OffTimerDelayNP{1};
        INDI::PropertySwitch OffTimerStartSP{4};
        INDI::PropertySwitch OffTimerCancelSP{4};
        INDI::PropertyNumber OffTimerRemainingNP{3};
};
