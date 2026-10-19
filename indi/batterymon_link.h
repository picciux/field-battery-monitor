#pragma once

// Livello protocollo verso il firmware ESP32 (websocket, porta 81).
// Nessuna dipendenza da INDI: si puo' testare a parte.
//
// Threading: IXWebSocket chiama il callback in un thread proprio. Qui il
// callback si limita ad aggiornare uno State protetto da mutex e ad alzare
// dei "dirty bit"; il driver INDI (single-thread) li consuma in TimerHit()
// tramite snapshot(). Il callback NON deve mai loggare/toccare proprieta' INDI.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

#include <ixwebsocket/IXWebSocket.h>

// Stessa convenzione dei driver INDI: di default si usa il nlohmann::json
// distribuito con libindi (indijson.hpp); con -D_USE_SYSTEM_JSONLIB quello di sistema.
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

namespace bm
{

    constexpr int MAX_OUTLETS = 3;   // = MAX_OUTLETS di board.h nel firmware

    // Payload dell'evento {"type":"capabilities"} (vedi WifiComm::sendCaps)
    struct Capabilities
    {
        int channels = 0;
        bool light = false;
        int outlets = 0;
        int lightAutoBrMinPct = 1;
        int lightAutoDrMin = 10;
        int lightAutoDrMax = 60;
        int cpLtMin = -10;
        int cpLtMax = 10;
        float socRvMin = 13.9f;
        float socRvMax = 14.6f;
        std::string fwVer;
        std::string resetReason;
        uint32_t bootCount = 0;
        uint32_t uptimeS = 0;
    };

    struct Settings
    {
        // we're interested in system name only
        std::string systemName;
    };

    // Ultimo stato noto del dispositivo (uno per evento del protocollo)
    struct State
    {
        bool linkUp = false;
        std::string lastError;

        bool haveCaps = false;
        Capabilities caps;

        bool haveSettings = false;
        Settings stgs;

        // battery_update. voltage/current/temperature possono essere null nel JSON
        bool haveBattery = false;
        bool sensorOk = false;         // battery_sensor_ok (INA226)
        bool voltageValid = false;
        bool currentValid = false;
        bool tempValid = false;        // DS18B20
        double voltage = 0.0;
        double current = 0.0;          // convenzione firmware: negativo = scarica
        double soc = 0.0;
        double temperature = 0.0;
        std::chrono::steady_clock::time_point lastBatteryRx{};

        // battery_autonomy_update
        bool haveAutonomy = false;
        double autonomyH = 0.0;        // tetto 24 h = "in carica / consumo trascurabile"

        // safety_update
        bool haveSafety = false;
        bool isSafe = true;

        // cold_protection_update
        bool haveCold = false;
        double coldLt = 0.0;

        bool haveSocRV = false;
        double socRV = 0.0;

        // light_update (0..1 per le luminosita')
        bool haveLight = false;
        double lightBrightness = 0.0;
        bool lightAuto = false;
        double lightAutoBr = 0.0;
        int lightAutoDr = 0;

        // outlet_update
        bool haveOutlet[MAX_OUTLETS] = {};
        double outletPower[MAX_OUTLETS] = {};   // 0..1

        // {"type":"result","payload":false} ricevuti
        uint32_t rejectedCommands = 0;
    };

    enum Dirty : uint32_t
    {
        D_LINK     = 1u << 0,
        D_CAPS     = 1u << 1,
        D_BATTERY  = 1u << 2,
        D_AUTONOMY = 1u << 3,
        D_SAFETY   = 1u << 4,
        D_COLD     = 1u << 5,
        D_SOC_RV   = 1u << 6,
        D_LIGHT    = 1u << 7,
        D_OUTLET   = 1u << 8,
        D_RESULT   = 1u << 9,
        D_SETTINGS = 1u << 10
    };

    class Link
    {
    public:
        Link();
        ~Link();

        void setUrl(const std::string &host, int port);

        // Azzera lo stato e avvia il thread del websocket (con riconnessione automatica)
        void start();
        void stop();

        // Attende la prima "capabilities" (il firmware la invia subito dopo l'handshake)
        bool waitForCaps(std::chrono::milliseconds timeout);

        bool isOpen() const;

        // Copia lo stato corrente SENZA consumare i dirty bit
        bool getCaps(Capabilities &out) const;

        // Copia lo stato corrente, restituisce e azzera i dirty bit
        uint32_t snapshot(State &out);

        // --- Comandi (vedi websocket_proto.h del firmware). Ritornano true se
        // --- il messaggio e' stato accodato; l'esito reale arriva come evento.
        bool getSettings();
        bool socReset();
        bool setColdThreshold(double celsius);
        bool setSocResetVoltage(double volt);
        bool setLightBrightness(double b01);
        bool setLightAuto(bool enabled);
        bool setLightAutoBrightness(double b01);
        bool setLightAutoDuration(int seconds);
        bool setOutletPower(int index, double p01);
        bool restartDevice();

    private:
        bool sendJson(const nlohmann::json &j);
        void onMessage(const ix::WebSocketMessagePtr &msg);
        void handleText(const std::string &text);

        ix::WebSocket ws_;
        std::string url_;

        mutable std::mutex m_;
        std::condition_variable cv_;
        State st_;
        uint32_t dirty_ = 0;
    };

} // namespace bm
