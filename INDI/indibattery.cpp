#include "indibattery.h"

FieldBattery::FieldBattery() {
    setVersion(1, 0);
    socSogliaMinima = 20.0;
    strncpy(ipAddress, "192.168.1.100", sizeof(ipAddress));
    curl_global_init(CURL_GLOBAL_ALL);
    curlCtx = curl_easy_init();
}

FieldBattery::~FieldBattery() {
    if (curlCtx) curl_easy_cleanup(curlCtx);
    curl_global_cleanup();
}

const char* FieldBattery::getDefaultName() { return "Field Battery Power Station"; }

// Specifichiamo l'interfaccia corretta: POWER + SAFETY
uint16_t FieldBattery::getInterface() {
    return INDI_INTERFACE_POWER | INDI_INTERFACE_SAFETY;
}

bool FieldBattery::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Proprietà di connessione e soglie (come prima)
    IUFillNumber(&IPAddressN, "IP", "Indirizzo IP", "%s", 0, 0, 0, 0);
    IUFillNumberVector(&IPAddressNP, IPAddressN, 1, getDeviceName(), "SERVER_CONFIG", "Connessione", "Main", IP_RW, 0, IPS_IDLE);

    IUFillNumber(&SocThresholdN, "THRESHOLD", "Soglia Minima SoC (%)", "%2.0f", 5, 50, 1, socSogliaMinima);
    IUFillNumberVector(&SocThresholdNP, SocThresholdN, 1, getDeviceName(), "SAFETY_CONFIG", "Configurazione Allarme", "Main", IP_RW, 0, IPS_IDLE);

    IUFillLight(&SafetyStatusL, "SAFETY", "Stato Alimentazione", IPS_OK);
    IUFillLightVector(&SafetyStatusLP, SafetyStatusL, 1, getDeviceName(), "SAFETY_STATUS", "Sicurezza Rig", "Main", IPS_OK);

    // INIZIALIZZAZIONE PROPRIETÀ STANDARD INDI "POWER"
    // Nota: I nomi dei vettori come "POWER_OUTLET" o "POWER_PORT_VOLTAGE" sono stringhe standard riconosciute da Ekos.

    // 1. Canale di switch standard (Uscita 2 del firmware)
    IUFillSwitch(&PowerOutletS[0], "OUTLET_1", "Uscita 2 (12V Aux)", ISS_OFF);
    IUFillSwitchVector(&PowerOutletSP, PowerOutletS, 1, getDeviceName(), "POWER_OUTLET", "Interruttori", "Power", ISR_1OFMANY, 0, IPS_IDLE);

    // 2. Canale PWM standard (Uscita 1 Dimmer del firmware)
    IUFillNumber(&PowerOutletPwmN[0], "PWM_1", "Uscita 1 (Dimmer %)", "%3.0f", 0, 100, 1, 0);
    IUFillNumberVector(&PowerOutletPwmNP, PowerOutletPwmN, 1, getDeviceName(), "POWER_OUTLET_PWM", "Regolazione PWM", "Power", IP_RW, 0, IPS_IDLE);

    // 3. Lettura Tensione Standard
    IUFillNumber(&PowerVoltageN[0], "VOLTAGE_MAIN", "Tensione Batteria (V)", "%2.2f", 0, 20, 0, 0);
    IUFillNumberVector(&PowerVoltageNP, PowerVoltageN, 1, getDeviceName(), "POWER_PORT_VOLTAGE", "Voltmetro", "Power", IP_RO, 0, IPS_IDLE);

    // 4. Lettura Corrente Standard
    IUFillNumber(&PowerCurrentN[0], "CURRENT_MAIN", "Assorbimento Totale (A)", "%1.3f", -20, 20, 0, 0);
    IUFillNumberVector(&PowerCurrentNP, PowerCurrentN, 1, getDeviceName(), "POWER_PORT_CURRENT", "Amperometro", "Power", IP_RO, 0, IPS_IDLE);

    // 5. Metriche Extra specifiche per la batteria (Tab separato "Battery Status")
    IUFillNumber(&ExtraMetricsN[0], "SOC", "Carica Residua (%)", "%3.1f", 0, 100, 0, 0);
    IUFillNumber(&ExtraMetricsN[1], "AUTONOMY", "Autonomia Stata (h)", "%3.1f", 0, 1000, 0, 0);
    IUFillNumber(&ExtraMetricsN[2], "TEMP", "Temperatura Cella (°C)", "%2.1f", -20, 60, 0, 0);
    IUFillNumberVector(&ExtraMetricsNP, ExtraMetricsN, 3, getDeviceName(), "BATTERY_METRICS", "Stato Accumulatore", "Battery Status", IP_RO, 0, IPS_IDLE);

    // Definizione delle proprietà sul server
    defineProperty(&IPAddressNP);
    defineProperty(&SocThresholdNP);
    defineProperty(&SafetyStatusLP);
    defineProperty(&PowerOutletSP);
    defineProperty(&PowerOutletPwmNP);
    defineProperty(&PowerVoltageNP);
    defineProperty(&PowerCurrentNP);
    defineProperty(&ExtraMetricsNP);

    SetTimer(2000);
    return true;
}

// Gestione dei cambi PWM inviati da Ekos
bool FieldBattery::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(name, "POWER_OUTLET_PWM") == 0) {
        IUUpdateNumber(&PowerOutletPwmNP, values, names, n);

        // Estrae il valore inviato dallo slider PWM (0-100)
        double pwmVal = PowerOutletPwmN[0].value;
        std::string url = "http://" + std::string(ipAddress) + "/number/channel_3_dimmer/set?value=" + std::to_string(pwmVal);

        if (httpPost(url)) PowerOutletPwmNP.s = IPS_OK;
        else PowerOutletPwmNP.s = IPS_ALERT;

        IDSetNumber(&PowerOutletPwmNP, nullptr);
        return true;
    }
    // ... mantieni la gestione di SAFETY_CONFIG vista prima ...
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

// Gestione dell'accensione/spegnimento switch inviato da Ekos
bool FieldBattery::ISNewSwitch(const char *dev, const char *name, ISState states[], char *names[], int n)
{
    if (strcmp(name, "POWER_OUTLET") == 0) {
        IUUpdateSwitch(&PowerOutletSP, states, names, n);

        std::string action = (PowerOutletS[0].s == ISS_ON) ? "turn_on" : "turn_off";
        std::string url = "http://" + std::string(ipAddress) + "/switch/channel_4_switch/" + action;

        if (httpPost(url)) PowerOutletSP.s = IPS_OK;
        else PowerOutletSP.s = IPS_ALERT;

        IDSetSwitch(&PowerOutletSP, nullptr);
        return true;
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

void FieldBattery::queryHardware()
{
    std::string url = "http://" + std::string(ipAddress) + "/json";
    std::string jsonStr = httpGet(url);

    if (jsonStr.empty()) {
        SafetyStatusL.s = IPS_ALERT;
        IDSetLight(&SafetyStatusLP, "Disconnesso dall'ESP32!");
        PowerVoltageNP.s = IPS_ALERT; IDSetNumber(&PowerVoltageNP, nullptr);
        PowerCurrentNP.s = IPS_ALERT; IDSetNumber(&PowerCurrentNP, nullptr);
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (reader->parse(jsonStr.c_str(), jsonStr.c_str() + jsonStr.size(), &root, &errs)) {
        double attualeSoC = 100.0;

        for (const auto& entity : root) {
            std::string id = entity["id"].asString();

            // Mappatura sulle proprietà standard dell'interfaccia Power
            if (id == "sensor-battery_voltage")
                PowerVoltageN[0].value = entity["value"].asDouble();
            else if (id == "sensor-ist_current")
                PowerCurrentN[0].value = entity["value"].asDouble();

            // Mappatura sulle metriche custom della batteria
            else if (id == "sensor-soc_percentage_output") {
                ExtraMetricsN[0].value = entity["value"].asDouble();
                attualeSoC = ExtraMetricsN[0].value;
            }
            else if (id == "sensor-time_remaining_output")
                ExtraMetricsN[1].value = entity["value"].asDouble();
            else if (id == "sensor-temp_battery_bay")
                ExtraMetricsN[2].value = entity["value"].asDouble();
        }

        // Pubblica gli aggiornamenti a INDI
        PowerVoltageNP.s = IPS_OK;  IDSetNumber(&PowerVoltageNP, nullptr);
        PowerCurrentNP.s = IPS_OK;  IDSetNumber(&PowerCurrentNP, nullptr);
        ExtraMetricsNP.s = IPS_OK;  IDSetNumber(&ExtraMetricsNP, nullptr);

        // Controllo della sicurezza per l'interruttore automatico di Ekos
        if (attualeSoC <= socSogliaMinima) {
            SafetyStatusL.s = IPS_ALERT;
            IDSetLight(&SafetyStatusLP, "EMERGENZA: Batteria in esaurimento!");
        } else {
            SafetyStatusL.s = IPS_OK;
            IDSetLight(&SafetyStatusLP, "Tensioni e cariche nominali.");
        }
    }
}
