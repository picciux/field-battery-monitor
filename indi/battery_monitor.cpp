#include "battery_monitor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

// Alimenta la property POWER_SENSORS del PowerInterface con V e I della batteria.
// I nomi (PowerSensorsNP, SENSOR_VOLTAGE, SENSOR_CURRENT) sono ricostruiti a
// memoria: se non compilano, controllare indipowerinterface.h e correggere
// SOLO pushPowerSensors(), oppure mettere 0 qui (i dati restano in BATTERY_STATE).
#ifndef BATTERYMON_FEED_POWER_SENSORS
#define BATTERYMON_FEED_POWER_SENSORS 1
#endif

namespace
{
constexpr const char *DEFAULT_HOST = "battery-monitor.local";
constexpr const char *DEFAULT_PORT = "81";

constexpr int CONNECT_TIMEOUT_MS = 5000;
// Se l'ESP32 non pubblica battery_update da piu' di cosi' (normalmente 1/s),
// il dato e' considerato vecchio.
constexpr std::chrono::milliseconds BATTERY_STALE{10000};

// Stato di SAFETY_STATUS quando il link websocket e' caduto.
// IPS_IDLE = "sconosciuto" (semantica INDI). Se si preferisce che la perdita
// di collegamento fermi la sessione, usare IPS_ALERT.
constexpr IPState LINK_LOST_SAFETY_STATE = IPS_IDLE;

const char *BATTERY_TAB = "Battery";
const char *LIGHT_TAB   = "Light";
const char *DIAG_TAB    = "Diagnostics";

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

const char *stateName(IPState s)
{
    switch (s)
    {
        case IPS_OK:    return "Ok";
        case IPS_BUSY:  return "Busy";
        case IPS_ALERT: return "Alert";
        default:        return "Idle";
    }
}
} // namespace

static std::unique_ptr<BatteryMonitor> driver(new BatteryMonitor());

// ============================================================ ciclo di vita

BatteryMonitor::BatteryMonitor() : INDI::PowerInterface(this)
{
    setVersion(0, 1);
    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE);
}

const char *BatteryMonitor::getDefaultName()
{
    // Questo e' il nome da usare in CONNECTION_STRINGS del driver Safety Monitor
    return "Battery Monitor";
}

bool BatteryMonitor::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDefaultPollingPeriod(500);

    // NOTA: PowerInterface::initProperties() NON si chiama qui. Il numero di
    // porte dipende dalla variante del firmware (luce si'/no, 0..3 outlet), che
    // si conosce solo dopo la connessione: vedi setupPowerInterface().

    // ---- indirizzo del firmware (editabile solo da disconnessi)
    ServerAddressTP[ADDR_HOST].fill("HOST", "Host", DEFAULT_HOST);
    ServerAddressTP[ADDR_PORT].fill("PORT", "Port", DEFAULT_PORT);
    ServerAddressTP.fill(getDeviceName(), "SERVER_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    // ---- batteria
    BatteryNP[B_VOLTAGE].fill("VOLTAGE", "Voltage (V)", "%6.2f", 0, 20, 0, 0);
    BatteryNP[B_CURRENT].fill("CURRENT", "Current (A, - = discharge)", "%6.3f", -100, 100, 0, 0);
    BatteryNP[B_SOC].fill("SOC", "State of charge (%)", "%5.1f", 0, 100, 0, 0);
    BatteryNP[B_TEMPERATURE].fill("TEMPERATURE", "Temperature (C)", "%5.1f", -40, 85, 0, 0);
    BatteryNP[B_AUTONOMY].fill("AUTONOMY", "Autonomy (h, 24 = charging/idle)", "%5.1f", 0, 24, 0, 0);
    BatteryNP.fill(getDeviceName(), "BATTERY_STATE", "Battery", BATTERY_TAB, IP_RO, 0, IPS_IDLE);

    SensorsLP[S_INA226].fill("INA226", "Current/voltage sensor", IPS_IDLE);
    SensorsLP[S_DS18B20].fill("DS18B20", "Temperature sensor", IPS_IDLE);
    SensorsLP.fill(getDeviceName(), "BATTERY_SENSORS", "Sensors", BATTERY_TAB, IPS_IDLE);

    // ---- SAFETY_STATUS standard (Light, elemento SAFETY)
    SafetyStatusLP[0].fill("SAFETY", "Safety", IPS_IDLE);
    SafetyStatusLP.fill(getDeviceName(), "SAFETY_STATUS", "Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // ---- cold protection (soglia di attacco riscaldatore). Limiti aggiornati dalle capabilities.
    ColdProtectionNP[0].fill("LOW_THRESHOLD", "Heater on below (C)", "%3.0f", -10, 10, 1, 5);
    ColdProtectionNP.fill(getDeviceName(), "COLD_PROTECTION", "Cold protection", BATTERY_TAB, IP_RW, 0, IPS_IDLE);

    // ---- automazione luce (la luminosita' manuale e' la porta 0 del PowerInterface)
    LightAutoSP[AUTO_ON].fill("ON", "Enabled", ISS_OFF);
    LightAutoSP[AUTO_OFF].fill("OFF", "Disabled", ISS_ON);
    LightAutoSP.fill(getDeviceName(), "LIGHT_AUTO", "Motion automation", LIGHT_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    LightAutoNP[LA_BRIGHTNESS].fill("BRIGHTNESS", "On brightness (%)", "%3.0f", 1, 100, 1, 25);
    LightAutoNP[LA_DURATION].fill("DURATION", "On duration (s)", "%3.0f", 10, 60, 1, 30);
    LightAutoNP.fill(getDeviceName(), "LIGHT_AUTO_SETTINGS", "Automation settings", LIGHT_TAB, IP_RW, 0, IPS_IDLE);

    // ---- reset SoC a 100%
    SocResetSP[0].fill("RESET", "Set SoC to 100%", ISS_OFF);
    SocResetSP.fill(getDeviceName(), "SOC_RESET", "SoC reset", BATTERY_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // ---- diagnostica firmware (utile per tenere d'occhio i riavvii)
    FirmwareTP[FW_VERSION].fill("VERSION", "Firmware", "");
    FirmwareTP[FW_RESET_REASON].fill("RESET_REASON", "Last reset reason", "");
    FirmwareTP[FW_BOOT_COUNT].fill("BOOT_COUNT", "Boot count", "");
    FirmwareTP[FW_UPTIME].fill("UPTIME", "Uptime", "");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", DIAG_TAB, IP_RO, 0, IPS_IDLE);

    addDebugControl();
    addPollPeriodControl();

    return true;
}

void BatteryMonitor::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Sempre visibile, anche da disconnessi
    defineProperty(ServerAddressTP);
    loadConfig(true, "SERVER_ADDRESS");
}

bool BatteryMonitor::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        setupPowerInterface();

        // Limiti dal firmware, prima di definire le proprieta'
        ColdProtectionNP[0].setMin(caps_.cpLtMin);
        ColdProtectionNP[0].setMax(caps_.cpLtMax);
        LightAutoNP[LA_BRIGHTNESS].setMin(caps_.lightAutoBrMinPct);
        LightAutoNP[LA_BRIGHTNESS].setMax(100);
        LightAutoNP[LA_DURATION].setMin(caps_.lightAutoDrMin);
        LightAutoNP[LA_DURATION].setMax(caps_.lightAutoDrMax);

        defineProperty(BatteryNP);
        defineProperty(SensorsLP);
        defineProperty(SafetyStatusLP);
        defineProperty(ColdProtectionNP);
        if (hasLight_)
        {
            defineProperty(LightAutoSP);
            defineProperty(LightAutoNP);
        }
        defineProperty(SocResetSP);
        defineProperty(FirmwareTP);

        INDI::PowerInterface::updateProperties();
    }
    else
    {
        deleteProperty(BatteryNP);
        deleteProperty(SensorsLP);
        deleteProperty(SafetyStatusLP);
        deleteProperty(ColdProtectionNP);
        if (hasLight_)
        {
            deleteProperty(LightAutoSP);
            deleteProperty(LightAutoNP);
        }
        deleteProperty(SocResetSP);
        deleteProperty(FirmwareTP);

        INDI::PowerInterface::updateProperties();
    }

    return true;
}

bool BatteryMonitor::Connect()
{
    const std::string host = ServerAddressTP[ADDR_HOST].getText();
    const int port = std::atoi(ServerAddressTP[ADDR_PORT].getText());
    if (host.empty() || port <= 0 || port > 65535)
    {
        LOG_ERROR("Indirizzo del firmware non valido.");
        return false;
    }

    LOGF_INFO("Connessione a ws://%s:%d ...", host.c_str(), port);
    link_.setUrl(host, port);
    link_.start();

    // Il firmware invia "capabilities" subito dopo l'handshake: da li' sappiamo
    // quante porte esporre.
    if (!link_.waitForCaps(std::chrono::milliseconds(CONNECT_TIMEOUT_MS)))
    {
        LOG_ERROR("Nessuna risposta dal firmware (capabilities non ricevute). Controllare host/porta e che il "
                  "dispositivo non abbia troppi client websocket aperti.");
        link_.stop();
        return false;
    }

    link_.getCaps(caps_);
    hasLight_ = caps_.light;
    outlets_  = std::min(caps_.outlets, bm::MAX_OUTLETS);
    linkWasUp_ = true;

    LOGF_INFO("Firmware v%s: %d canali, luce %s, %d outlet.", caps_.fwVer.c_str(), caps_.channels,
              hasLight_ ? "si'" : "no", outlets_);
    noteBootCount(caps_);

    SetTimer(getCurrentPollingPeriod());
    return true;
}

bool BatteryMonitor::Disconnect()
{
    link_.stop();
    return true;
}

bool BatteryMonitor::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    ServerAddressTP.save(fp);
    return INDI::PowerInterface::saveConfigItems(fp);
}

// ================================================= PowerInterface (porte)

void BatteryMonitor::setupPowerInterface()
{
    const int nPorts = (hasLight_ ? 1 : 0) + outlets_;

    if (powerInitDone_)
    {
        if (nPorts != powerInitPorts_)
            LOG_WARN("La variante hardware del firmware e' cambiata dall'ultima connessione: riavviare il driver "
                     "per aggiornare le porte Power.");
        return;
    }

    uint32_t cap = POWER_HAS_VOLTAGE_SENSOR | POWER_HAS_OVERALL_CURRENT;
    if (nPorts > 0)
        cap |= POWER_HAS_DEW_OUT;
    INDI::PowerInterface::SetCapability(cap);

    // VERIFICARE: l'esempio ufficiale la chiama in initProperties(); qui e'
    // differita alla connessione perche' il numero di porte e' dinamico. Se
    // non fosse supportato, ripiegare su un numero fisso di porte (es. 1 + 3).
    INDI::PowerInterface::initProperties("Power", 0, nPorts, 0, 0, 0);

    powerInitDone_ = true;
    powerInitPorts_ = nPorts;
}

// Layout porte: [Light se presente] [Outlet 0..N-1]
bool BatteryMonitor::resolvePort(size_t port, PortTarget &t) const
{
    int idx = (int) port;
    if (hasLight_)
    {
        if (idx == 0)
        {
            t = {true, -1};
            return true;
        }
        idx--;
    }
    if (idx >= 0 && idx < outlets_)
    {
        t = {false, idx};
        return true;
    }
    return false;
}

bool BatteryMonitor::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    PortTarget t;
    if (!resolvePort(port, t))
        return false;

    const double p = enabled ? clamp01(dutyCycle / 100.0) : 0.0;
    return t.isLight ? link_.setLightBrightness(p) : link_.setOutletPower(t.outlet, p);
}

// Non supportati dall'hardware
bool BatteryMonitor::SetPowerPort(size_t, bool)          { return false; }
bool BatteryMonitor::SetVariablePort(size_t, bool, double) { return false; }
bool BatteryMonitor::SetLEDEnabled(bool)                 { return false; }
bool BatteryMonitor::SetAutoDewEnabled(size_t, bool)     { return false; }
bool BatteryMonitor::CyclePower()                        { return false; }
bool BatteryMonitor::SetUSBPort(size_t, bool)            { return false; }

void BatteryMonitor::pushPowerSensors(const bm::State &s)
{
#if BATTERYMON_FEED_POWER_SENSORS
    if (!s.haveBattery)
        return;
    if (s.voltageValid)
        PowerSensorsNP[SENSOR_VOLTAGE].setValue(s.voltage);
    if (s.currentValid)
        // Il PowerInterface parla di corrente *assorbita*: in carica (I > 0) riportiamo 0.
        // Il valore con segno resta in BATTERY_STATE.
        PowerSensorsNP[SENSOR_CURRENT].setValue(std::max(0.0, -s.current));
    PowerSensorsNP.setState(s.sensorOk ? IPS_OK : IPS_ALERT);
    PowerSensorsNP.apply();
#else
    (void) s;
#endif
}

void BatteryMonitor::mirrorPortsToPowerInterface(const bm::State &s)
{
    // TODO: riflettere sulla UI del PowerInterface lo stato reale delle porte
    // (luce / outlet) quando cambia lato firmware (web UI, PIR, altro client).
    // Dipende dai nomi delle property interne di indipowerinterface.h
    // (duty cycle e switch on/off delle porte Dew): da completare guardando
    // l'header o un driver gia' migrato (es. Pegasus).
    // Dati disponibili: s.lightBrightness (0..1) -> porta 0 se hasLight_,
    //                   s.outletPower[i]  (0..1) -> porta (hasLight_ ? 1 : 0) + i
    (void) s;
}

// ============================================================ client -> device

bool BatteryMonitor::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (INDI::PowerInterface::processText(dev, name, texts, names, n))
            return true;

        if (ServerAddressTP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Disconnettere il driver prima di cambiare indirizzo.");
                ServerAddressTP.setState(IPS_ALERT);
            }
            else
            {
                ServerAddressTP.update(texts, names, n);
                ServerAddressTP.setState(IPS_OK);
                saveConfig(true, "SERVER_ADDRESS");
            }
            ServerAddressTP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool BatteryMonitor::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (INDI::PowerInterface::processNumber(dev, name, values, names, n))
            return true;

        if (ColdProtectionNP.isNameMatch(name))
        {
            ColdProtectionNP.update(values, names, n);
            const bool ok = link_.setColdThreshold(ColdProtectionNP[0].getValue());
            ColdProtectionNP.setState(ok ? IPS_OK : IPS_ALERT);
            ColdProtectionNP.apply();
            return true;
        }

        if (LightAutoNP.isNameMatch(name))
        {
            LightAutoNP.update(values, names, n);
            bool ok = true;
            // Inviamo solo gli elementi toccati dal client
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], LightAutoNP[LA_BRIGHTNESS].getName()))
                    ok &= link_.setLightAutoBrightness(clamp01(LightAutoNP[LA_BRIGHTNESS].getValue() / 100.0));
                else if (!strcmp(names[i], LightAutoNP[LA_DURATION].getName()))
                    ok &= link_.setLightAutoDuration((int) std::lround(LightAutoNP[LA_DURATION].getValue()));
            }
            LightAutoNP.setState(ok ? IPS_OK : IPS_ALERT);
            LightAutoNP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool BatteryMonitor::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (INDI::PowerInterface::processSwitch(dev, name, states, names, n))
            return true;

        if (LightAutoSP.isNameMatch(name))
        {
            LightAutoSP.update(states, names, n);
            const bool on = LightAutoSP[AUTO_ON].getState() == ISS_ON;
            const bool ok = link_.setLightAuto(on);
            LightAutoSP.setState(ok ? IPS_OK : IPS_ALERT);
            LightAutoSP.apply();
            return true;
        }

        if (SocResetSP.isNameMatch(name))
        {
            SocResetSP.update(states, names, n);
            const bool requested = SocResetSP[0].getState() == ISS_ON;
            bool ok = true;
            if (requested)
            {
                ok = link_.socReset();
                if (ok)
                    LOG_INFO("Richiesto reset del SoC al 100%.");
            }
            SocResetSP.reset();   // e' un pulsante, non uno stato
            SocResetSP.setState(ok ? IPS_OK : IPS_ALERT);
            SocResetSP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

// ============================================================ device -> client

void BatteryMonitor::TimerHit()
{
    if (!isConnected())
        return;

    bm::State s;
    const uint32_t dirty = link_.snapshot(s);
    applySnapshot(s, dirty);

    SetTimer(getCurrentPollingPeriod());
}

IPState BatteryMonitor::evaluateSafety(const bm::State &s) const
{
    if (!s.linkUp)
        return LINK_LOST_SAFETY_STATE;
    if (!s.haveSafety)
        return IPS_IDLE;
    if (!s.isSafe)
        return IPS_ALERT;

    // "Safe", ma il firmware congela IsSafe se l'INA226 e' guasto, e un dato
    // vecchio non e' un dato: segnaliamo warning, non "tutto ok".
    const bool stale = !s.haveBattery ||
                       (std::chrono::steady_clock::now() - s.lastBatteryRx) > BATTERY_STALE;
    if (!s.sensorOk || stale)
        return IPS_BUSY;

    return IPS_OK;
}

void BatteryMonitor::noteBootCount(const bm::Capabilities &c)
{
    if (lastBootCount_ != 0 && c.bootCount > lastBootCount_)
        LOGF_WARN("Il firmware si e' riavviato (boot_count %u -> %u, reset_reason=%s).", lastBootCount_,
                  c.bootCount, c.resetReason.c_str());
    lastBootCount_ = c.bootCount;
}

void BatteryMonitor::applySnapshot(const bm::State &s, uint32_t d)
{
    using namespace bm;

    // ---- collegamento
    if (d & D_LINK)
    {
        if (s.linkUp && !linkWasUp_)
            LOG_INFO("Collegamento websocket ripristinato.");
        else if (!s.linkUp && linkWasUp_)
            LOG_WARN("Collegamento websocket perso, riconnessione in corso...");
        linkWasUp_ = s.linkUp;

        if (!s.linkUp)
        {
            BatteryNP.setState(IPS_IDLE);
            BatteryNP.apply();
        }
        if (!s.lastError.empty() && s.lastError != lastLoggedError_)
        {
            LOGF_DEBUG("WebSocket: %s", s.lastError.c_str());
            lastLoggedError_ = s.lastError;
        }
    }

    // ---- capabilities (a ogni (ri)connessione del websocket)
    if (d & D_CAPS)
    {
        if (s.caps.light != caps_.light || std::min(s.caps.outlets, MAX_OUTLETS) != outlets_)
            LOG_WARN("La variante hardware del firmware e' cambiata: riavviare il driver.");
        noteBootCount(s.caps);

        FirmwareTP[FW_VERSION].setText(s.caps.fwVer.c_str());
        FirmwareTP[FW_RESET_REASON].setText(s.caps.resetReason.c_str());
        FirmwareTP[FW_BOOT_COUNT].setText(std::to_string(s.caps.bootCount).c_str());

        char up[32];
        const uint32_t t = s.caps.uptimeS;
        snprintf(up, sizeof(up), "%ud %02u:%02u:%02u", t / 86400, (t / 3600) % 24, (t / 60) % 60, t % 60);
        FirmwareTP[FW_UPTIME].setText(up);

        FirmwareTP.setState(IPS_OK);
        FirmwareTP.apply();
    }

    // ---- batteria
    if (d & D_BATTERY)
    {
        if (s.voltageValid) BatteryNP[B_VOLTAGE].setValue(s.voltage);
        if (s.currentValid) BatteryNP[B_CURRENT].setValue(s.current);
        BatteryNP[B_SOC].setValue(s.soc);   // sempre presente (ultima stima nota)
        if (s.tempValid)    BatteryNP[B_TEMPERATURE].setValue(s.temperature);
        BatteryNP.setState(s.sensorOk ? IPS_OK : IPS_ALERT);
        BatteryNP.apply();

        const IPState ina = s.sensorOk ? IPS_OK : IPS_ALERT;
        const IPState ds  = s.tempValid ? IPS_OK : IPS_ALERT;
        if (SensorsLP[S_INA226].getState() != ina || SensorsLP[S_DS18B20].getState() != ds)
        {
            SensorsLP[S_INA226].setState(ina);
            SensorsLP[S_DS18B20].setState(ds);
            SensorsLP.setState((ina == IPS_OK && ds == IPS_OK) ? IPS_OK : IPS_ALERT);
            SensorsLP.apply();
            if (ina != IPS_OK)
                LOG_WARN("INA226 non risponde: tensione/corrente congelate, SoC e safety all'ultimo valore noto.");
        }

        pushPowerSensors(s);
    }

    if ((d & D_AUTONOMY) && s.haveAutonomy)
    {
        BatteryNP[B_AUTONOMY].setValue(s.autonomyH);
        BatteryNP.apply();
    }

    // ---- cold protection
    if ((d & D_COLD) && s.haveCold)
    {
        ColdProtectionNP[0].setValue(s.coldLt);
        ColdProtectionNP.setState(IPS_OK);
        ColdProtectionNP.apply();
    }

    // ---- luce e outlet
    if ((d & D_LIGHT) && hasLight_ && s.haveLight)
    {
        LightAutoSP[AUTO_ON].setState(s.lightAuto ? ISS_ON : ISS_OFF);
        LightAutoSP[AUTO_OFF].setState(s.lightAuto ? ISS_OFF : ISS_ON);
        LightAutoSP.setState(IPS_OK);
        LightAutoSP.apply();

        LightAutoNP[LA_BRIGHTNESS].setValue(s.lightAutoBr * 100.0);
        LightAutoNP[LA_DURATION].setValue(s.lightAutoDr);
        LightAutoNP.setState(IPS_OK);
        LightAutoNP.apply();
    }
    if (d & (D_LIGHT | D_OUTLET))
        mirrorPortsToPowerInterface(s);

    // ---- comandi rifiutati dal firmware
    if (d & D_RESULT)
        LOGF_WARN("Il firmware ha rifiutato un comando (totale rifiutati: %u).", s.rejectedCommands);

    // ---- SAFETY_STATUS: valutata a ogni tick, cosi' cattura anche il dato "vecchio"
    const IPState ns = evaluateSafety(s);
    if (ns != SafetyStatusLP.getState())
    {
        LOGF_INFO("SAFETY_STATUS: %s -> %s", stateName(SafetyStatusLP.getState()), stateName(ns));
        SafetyStatusLP[0].setState(ns);
        SafetyStatusLP.setState(ns);
        SafetyStatusLP.apply();
    }
}
