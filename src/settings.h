#ifndef SETTINGS_H
#define SETTINGS_H

//#define HOSTNAME_LEN          31  //30 chars + null-term
//#define SSID_LEN              33  //32 chars + null-term
//#define PSK_LEN               64  //63 chars + null-term

class Settings {
  public:
    static constexpr size_t HOSTNAME_MAX_LEN = 31;
    static constexpr size_t SSID_MAX_LEN = 33;
    static constexpr size_t PSK_MAX_LEN = 64;

    static constexpr size_t MAX_STRING_LEN = PSK_MAX_LEN;

    void setup();
    void factoryReset();

    char *getHostname();
    void setHostname(const char *hostname_);

    char *getDisplayName();
    void setDisplayName(const char *displayName_);

    char *getMainSsid();
    void setMainSsid(const char *mainSsid_);

    char *getMainPsk();
    void setMainPsk(const char *mainPsk_);

    char *getAltSsid();
    void setAltSsid(const char *altSsid_);

    char *getAltPsk();
    void setAltPsk(const char *altPsk_);

    char *getApPsk();
    void setApPsk(const char *apPsk_);

    bool isApDefaultGWDisabled();
    void setApDefaultGWDisabled(const bool &disabled);

    bool isColdProtectionEnabled();
    void setColdProtection(const bool &enable);

    float getCpLowThreshold();
    void setCpLowThreshold(const float &cpLowThreshold_);

    uint8_t getAutoLightDuration();
    void setAutoLightDuration(const uint8_t &autoLightDuration_);

    float getAutoLightBrightness();
    void setAutoLightBrightness(const float &autoLightBrightness_);

    bool isAutoLightEnabled();
    void setAutoLightEnabled(const bool &enabled);

  private:
    /* hostname: will be network SSID when acting as an AP */
    char hostname[HOSTNAME_MAX_LEN];
    char displayName[HOSTNAME_MAX_LEN];

    /* Network password when acting as an AP */
    char apPsk[PSK_MAX_LEN];

    /* Preferred network SSID and password to connect to */
    char mainSsid[SSID_MAX_LEN];
    char mainPsk[PSK_MAX_LEN];

    /* Alternative network SSID and password to connect to if main is not in range*/ 
    char altSsid[SSID_MAX_LEN];
    char altPsk[PSK_MAX_LEN];

    /* Don't be default gateway for clients when acting as an AP */
    bool apDontBeDefaultGW;

    /* light motion automation */
    bool autoLightEnabled;
    float autoLightBrightness;
    uint8_t autoLightDuration;

    /* battery cold protection */
    bool coldProtection;
    float cpLowThreshold;
};

#endif //SETTINGS_H
