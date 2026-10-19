#include "batterymon_link.h"

using json = nlohmann::json;

namespace bm
{

namespace
{
// Lettura tollerante: chiave assente, null o tipo sbagliato -> default.
template <typename T>
T getOr(const json &j, const char *key, T def)
{
    auto it = j.find(key);
    if (it == j.end())
        return def;
    try
    {
        return it->get<T>();
    }
    catch (const json::exception &)
    {
        return def;
    }
}

// true se la chiave esiste ed e' un numero (il firmware manda null a sensore guasto)
bool isNum(const json &j, const char *key)
{
    auto it = j.find(key);
    return it != j.end() && it->is_number();
}
} // namespace

Link::Link()
{
    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr &m)
    {
        onMessage(m);
    });
    // Il ping periodico serve a scoprire connessioni "a meta'" (es. rete caduta
    // senza FIN) e a farle richiudere/riaprire invece di restare zombie.
    ws_.setPingInterval(10);
    ws_.setHandshakeTimeout(5);
    ws_.setMinWaitBetweenReconnectionRetries(1000);
    ws_.setMaxWaitBetweenReconnectionRetries(10000);
    // links2004/WebSockets lato ESP32 non supporta permessage-deflate
    ws_.disablePerMessageDeflate();
}

Link::~Link()
{
    ws_.stop();
}

void Link::setUrl(const std::string &host, int port)
{
    url_ = "ws://" + host + ":" + std::to_string(port) + "/";
}

void Link::start()
{
    {
        std::lock_guard<std::mutex> lk(m_);
        st_ = State();
        dirty_ = 0;
    }
    ws_.setUrl(url_);
    ws_.start();
}

void Link::stop()
{
    // Fuori dal lock: stop() attende il thread del callback, che prende m_.
    ws_.stop();
    std::lock_guard<std::mutex> lk(m_);
    st_ = State();
    dirty_ = 0;
}

bool Link::waitForCaps(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lk(m_);
    return cv_.wait_for(lk, timeout, [this] { return st_.haveCaps; });
}

bool Link::isOpen() const
{
    return ws_.getReadyState() == ix::ReadyState::Open;
}

bool Link::getCaps(Capabilities &out) const
{
    std::lock_guard<std::mutex> lk(m_);
    out = st_.caps;
    return st_.haveCaps;
}

uint32_t Link::snapshot(State &out)
{
    std::lock_guard<std::mutex> lk(m_);
    out = st_;
    const uint32_t d = dirty_;
    dirty_ = 0;
    return d;
}

// ---------------------------------------------------------------- ricezione

void Link::onMessage(const ix::WebSocketMessagePtr &msg)
{
    switch (msg->type)
    {
        case ix::WebSocketMessageType::Open:
        {
            std::lock_guard<std::mutex> lk(m_);
            st_.linkUp = true;
            dirty_ |= D_LINK;
            break;
        }
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error:
        {
            std::lock_guard<std::mutex> lk(m_);
            st_.linkUp = false;
            if (msg->type == ix::WebSocketMessageType::Error)
                st_.lastError = msg->errorInfo.reason;
            dirty_ |= D_LINK;
            cv_.notify_all();
            break;
        }
        case ix::WebSocketMessageType::Message:
            handleText(msg->str);
            break;
        default:   // Ping, Pong, Fragment
            break;
    }
}

void Link::handleText(const std::string &text)
{
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object())
        return;

    std::lock_guard<std::mutex> lk(m_);

    // --- messaggi "type": capabilities, result, settings
    const std::string type = getOr<std::string>(j, "type", "");
    if (!type.empty())
    {
        if (type == "capabilities")
        {
            auto p = j.find("payload");
            if (p == j.end() || !p->is_object())
                return;
            Capabilities &c = st_.caps;
            c.channels          = (int) getOr<double>(*p, "channels", 0);
            c.light             = getOr<bool>(*p, "light", false);
            c.outlets           = (int) getOr<double>(*p, "outlets", 0);
            c.lightAutoBrMinPct = (int) getOr<double>(*p, "light_auto_br_min_pct", 1);
            c.lightAutoDrMin    = (int) getOr<double>(*p, "light_auto_dr_min", 10);
            c.lightAutoDrMax    = (int) getOr<double>(*p, "light_auto_dr_max", 60);
            c.cpLtMin           = (int) getOr<double>(*p, "cp_lt_min", -10);
            c.cpLtMax           = (int) getOr<double>(*p, "cp_lt_max", 10);
            c.socRvMin          = (float) getOr<double>(*p, "soc_rv_min", 13.9);
            c.socRvMax          = (float) getOr<double>(*p, "soc_rv_max", 14.6);
            c.fwVer             = getOr<std::string>(*p, "fw_ver", "");
            c.resetReason       = getOr<std::string>(*p, "reset_reason", "");
            c.bootCount         = (uint32_t) getOr<double>(*p, "boot_count", 0);
            c.uptimeS           = (uint32_t) getOr<double>(*p, "uptime_s", 0);
            st_.haveCaps = true;
            dirty_ |= D_CAPS;
            cv_.notify_all();
        }
        else if (type == "settings")
        {
            // Nessun id di correlazione: sappiamo solo che *un* comando e' stato rifiutato
            auto p = j.find("payload");
            if (p == j.end() || !p->is_object())
                return;
            Settings &s = st_.stgs;
            s.systemName        = getOr<std::string>(*p, "display_name", "");
            st_.haveSettings = true;
            dirty_ |= D_SETTINGS;
            cv_.notify_all();
        }
        else if (type == "result")
        {
            // Nessun id di correlazione: sappiamo solo che *un* comando e' stato rifiutato
            if (!getOr<bool>(j, "payload", true))
            {
                st_.rejectedCommands++;
                dirty_ |= D_RESULT;
            }
        }
        return;   // altri: non ci interessano
    }

    // --- eventi "event"
    const std::string ev = getOr<std::string>(j, "event", "");

    if (ev == "battery_update")
    {
        st_.haveBattery  = true;
        st_.sensorOk     = getOr<bool>(j, "battery_sensor_ok", false);
        st_.voltageValid = isNum(j, "voltage");
        st_.currentValid = isNum(j, "current");
        st_.tempValid    = isNum(j, "temperature");
        if (st_.voltageValid) st_.voltage     = getOr<double>(j, "voltage", 0.0);
        if (st_.currentValid) st_.current     = getOr<double>(j, "current", 0.0);
        if (st_.tempValid)    st_.temperature = getOr<double>(j, "temperature", 0.0);
        st_.soc = getOr<double>(j, "soc", st_.soc);
        st_.lastBatteryRx = std::chrono::steady_clock::now();
        dirty_ |= D_BATTERY;
    }
    else if (ev == "battery_autonomy_update")
    {
        st_.haveAutonomy = true;
        st_.autonomyH = getOr<double>(j, "hours", 0.0);
        dirty_ |= D_AUTONOMY;
    }
    else if (ev == "safety_update")
    {
        st_.haveSafety = true;
        st_.isSafe = getOr<bool>(j, "is_safe", true);
        dirty_ |= D_SAFETY;
    }
    else if (ev == "cold_protection_update")
    {
        st_.haveCold = true;
        st_.coldLt = getOr<double>(j, "lt", 0.0);
        dirty_ |= D_COLD;
    }
    else if (ev == "soc_sync_update")
    {
        st_.haveSocRV = true;
        st_.socRV = getOr<double>(j, "voltage", 0.0);
        dirty_ |= D_SOC_RV;
    }
    else if (ev == "light_update")
    {
        st_.haveLight       = true;
        st_.lightBrightness = getOr<double>(j, "brightness", 0.0);
        st_.lightAuto       = getOr<bool>(j, "auto", false);
        st_.lightAutoBr     = getOr<double>(j, "auto_br", 0.0);
        st_.lightAutoDr     = (int) getOr<double>(j, "auto_dr", 0);
        dirty_ |= D_LIGHT;
    }
    else if (ev == "outlet_update")
    {
        const int idx = (int) getOr<double>(j, "index", -1);
        if (idx >= 0 && idx < MAX_OUTLETS && isNum(j, "power"))
        {
            st_.haveOutlet[idx]  = true;
            st_.outletPower[idx] = getOr<double>(j, "power", 0.0);
            dirty_ |= D_OUTLET;
        }
    }
}

// ------------------------------------------------------------------- invio

bool Link::sendJson(const json &j)
{
    if (!isOpen())
        return false;
    return ws_.sendText(j.dump()).success;
}

bool Link::getSettings()
{
    return sendJson({{"action", "get_settings"}});
}

bool Link::socReset()
{
    return sendJson({{"action", "battery_soc_reset"}});
}

bool Link::setColdThreshold(double celsius)
{
    return sendJson({{"action", "cp_low_threshold"}, {"temperature", celsius}});
}

bool Link::setSocResetVoltage(double volt)
{
    return sendJson({{"action", "soc_reset_voltage"}, {"voltage", volt}});
}

bool Link::setLightBrightness(double b01)
{
    return sendJson({{"action", "light_brightness"}, {"brightness", b01}});
}

bool Link::setLightAuto(bool enabled)
{
    return sendJson({{"action", "light_auto_enabled"}, {"enabled", enabled}});
}

bool Link::setLightAutoBrightness(double b01)
{
    return sendJson({{"action", "light_auto_brightness"}, {"brightness", b01}});
}

bool Link::setLightAutoDuration(int seconds)
{
    return sendJson({{"action", "light_auto_duration"}, {"seconds", seconds}});
}

bool Link::setOutletPower(int index, double p01)
{
    return sendJson({{"action", "outlet_power"}, {"index", index}, {"power", p01}});
}

bool Link::restartDevice()
{
    return sendJson({{"action", "restart"}});
}

} // namespace bm
