#pragma once

#include <defaultdevice.h>
#include <curl/curl.h>
#include <json/json.h>
#include <string>

class FieldBattery : public INDI::DefaultDevice
{
public:
    FieldBattery();
    ~FieldBattery() override;

    const char *getDefaultName() override;
    uint16_t getInterface() override; // Ritornerà POWER | SAFETY

    bool initProperties() override;
    bool updateProperties() override;
    void TimerHit() override;

    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState states[], char *names[], int n) override;

private:
    // Connessione e Allarmi
    char ipAddress[64];
    INumber IPAddressN;
    NUM_E IPAddressNP;

    double socSogliaMinima;
    INumber SocThresholdN;
    NUM_E SocThresholdNP;

    // Sicurezza (SAFETY_STATUS)
    ISLight SafetyStatusL;
    ISLightVector SafetyStatusLP;

    // --- PROPRIETÀ STANDARD INTERFACCIA POWER ---
    // 1. Switch ON/OFF per le uscite (Outlet 1 = Uscita 2 del firmware)
    ISwitch PowerOutletS[1];
    SWITCH_E PowerOutletSP;

    // 2. Canali PWM Dimmerabili (PWM 1 = Uscita 1 Dimmer del firmware)
    INumber PowerOutletPwmN[1];
    NUM_E PowerOutletPwmNP;

    // 3. Sensori di Tensione e Corrente standard
    INumber PowerVoltageN[1];
    NUM_E PowerVoltageNP;

    INumber PowerCurrentN[1];
    NUM_E PowerCurrentNP;

    // 4. Sensori ausiliari (SoC, Autonomia, Temp) - proprietà custom nel tab del dispositivo
    INumber ExtraMetricsN[3];
    NUM_E ExtraMetricsNP;

    // HTTP Helper
    CURL *curlCtx;
    std::string httpGet(const std::string &url);
    bool httpPost(const std::string &url);
    void queryHardware();
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);
};
