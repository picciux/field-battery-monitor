
#include "include_config.h"

#include <Preferences.h>
#include "settings.h"

#define HOSTNAME            "hostname"
#define DISPLAY_NAME        "display_name"
#define MAIN_SSID           "main_ssid"
#define MAIN_PSK            "main_psk"
#define ALT_SSID            "alt_ssid"
#define ALT_PSK             "alt_psk"
#define AP_PSK              "ap_psk"
#define AP_NO_DEF_GW        "ap_no_def_gw"
#define COLD_PROTECTION     "cold_protection"
#define CP_LOW_THRESHOLD    "cp_low_threshold"
#define AUTO_LIGHT          "auto_light"
#define AUTO_LIGHT_BRIGHT   "al_brightness"
#define AUTO_LIGHT_DURATION "al_duration"

Preferences g_prefs;

char *Settings::getHostname() { return hostname; }
void Settings::setHostname(const char *hostname_) {
  if (strcmp(hostname, hostname_) == 0) return; 
  strlcpy(hostname, hostname_, Settings::HOSTNAME_MAX_LEN);
  g_prefs.putString(HOSTNAME, hostname); 
}

char *Settings::getDisplayName() { return displayName; }
void Settings::setDisplayName(const char *displayName_) { 
  if (strcmp(displayName, displayName_) == 0) return; 
  strlcpy(displayName, displayName_, Settings::HOSTNAME_MAX_LEN); 
  g_prefs.putString(DISPLAY_NAME, displayName); 
}

char *Settings::getMainSsid() { return mainSsid; }
void Settings::setMainSsid(const char *mainSsid_) { 
  if (strcmp(mainSsid, mainSsid_) == 0) return; 
  strlcpy(mainSsid, mainSsid_, Settings::SSID_MAX_LEN); 
  g_prefs.putString(MAIN_SSID, mainSsid); 
}

char *Settings::getMainPsk() { return mainPsk; }
void Settings::setMainPsk(const char *mainPsk_) { 
  if (strcmp(mainPsk, mainPsk_) == 0) return; 
  strlcpy(mainPsk, mainPsk_, Settings::PSK_MAX_LEN); 
  g_prefs.putString(MAIN_PSK, mainPsk); 
}

char *Settings::getAltSsid() { return altSsid; }
void Settings::setAltSsid(const char *altSsid_) { 
  if (strcmp(altSsid, altSsid_) == 0) return; 
  strlcpy(altSsid, altSsid_, Settings::SSID_MAX_LEN); 
  g_prefs.putString(ALT_SSID, altSsid); 
}

char *Settings::getAltPsk() { return altPsk; }
void Settings::setAltPsk(const char *altPsk_) { 
  if (strcmp(altPsk, altPsk_) == 0) return; 
  strlcpy(altPsk, altPsk_, Settings::PSK_MAX_LEN); 
  g_prefs.putString(ALT_PSK, altPsk); 
}

char *Settings::getApPsk() { return apPsk; }
void Settings::setApPsk(const char *apPsk_) { 
  if (strcmp(apPsk, apPsk_) == 0) return; 
  strlcpy(apPsk, apPsk_, Settings::PSK_MAX_LEN); 
  g_prefs.putString(AP_PSK, apPsk); 
}

bool Settings::isApDefaultGWDisabled() { return apDontBeDefaultGW; }
void Settings::setApDefaultGWDisabled(const bool &disabled) {
  if (disabled == apDontBeDefaultGW) return;
  apDontBeDefaultGW = disabled;
  g_prefs.putBool(AP_NO_DEF_GW, apDontBeDefaultGW); 
}

bool Settings::isColdProtectionEnabled() { return coldProtection; }
void Settings::setColdProtection(const bool &enable) { 
  if (enable == coldProtection) return;
  coldProtection = enable; 
  g_prefs.putBool(COLD_PROTECTION, coldProtection);
}

float Settings::getCpLowThreshold() { return cpLowThreshold; }
void Settings::setCpLowThreshold(const float &cpLowThreshold_) { 
  //TODO check valid value
  if (cpLowThreshold == cpLowThreshold_) return;
  cpLowThreshold = cpLowThreshold_;
  g_prefs.putFloat(CP_LOW_THRESHOLD, cpLowThreshold);
}

bool Settings::isAutoLightEnabled() {return autoLightEnabled; }
void Settings::setAutoLightEnabled(const bool &enabled) { 
  if (enabled == autoLightEnabled) return;
  autoLightEnabled = enabled;
  g_prefs.putBool(AUTO_LIGHT, autoLightEnabled);
}

float Settings::getAutoLightBrightness() { return autoLightBrightness; }
void Settings::setAutoLightBrightness(const float &autoLightBrightness_) { 
  if (autoLightBrightness == autoLightBrightness_) return;
  autoLightBrightness = autoLightBrightness_;
  g_prefs.putFloat(AUTO_LIGHT_BRIGHT, autoLightBrightness);
}

uint8_t Settings::getAutoLightDuration() { return autoLightDuration; }
void Settings::setAutoLightDuration(const uint8_t &autoLightDuration_) { 
  if (autoLightDuration == autoLightDuration_) return;
  autoLightDuration = autoLightDuration_;
  g_prefs.putUInt(AUTO_LIGHT_DURATION, autoLightDuration);
}

void Settings::setup() {
  g_prefs.begin("settings");
  
  strlcpy(hostname, g_prefs.getString(HOSTNAME, DEFAULT_HOSTNAME).c_str(), Settings::HOSTNAME_MAX_LEN);
  strlcpy(displayName, g_prefs.getString(DISPLAY_NAME, DEFAULT_DISPLAY_NAME).c_str(), Settings::HOSTNAME_MAX_LEN);
  
  strlcpy(mainSsid, g_prefs.getString(MAIN_SSID, DEFAULT_MAIN_SSID).c_str(), Settings::SSID_MAX_LEN);
  strlcpy(mainPsk, g_prefs.getString(MAIN_PSK, DEFAULT_MAIN_PSK).c_str(), Settings::PSK_MAX_LEN);
  
  strlcpy(altSsid, g_prefs.getString(ALT_SSID, DEFAULT_ALT_SSID).c_str(), Settings::SSID_MAX_LEN);
  strlcpy(altPsk, g_prefs.getString(ALT_PSK, DEFAULT_ALT_PSK).c_str(), Settings::PSK_MAX_LEN);
  
  strlcpy(apPsk, g_prefs.getString(AP_PSK, DEFAULT_AP_PSK).c_str(), Settings::PSK_MAX_LEN);
  
  apDontBeDefaultGW = g_prefs.getBool(AP_NO_DEF_GW, DEFAULT_AP_DONT_BE_DEF_GW);
  
  coldProtection = g_prefs.getBool(COLD_PROTECTION, DEFAULT_COLD_PROTECTION_ENABLED);
  cpLowThreshold = g_prefs.getFloat(CP_LOW_THRESHOLD, DEFAULT_COLT_PROTECTION_LOW_THRESHOLD);

  autoLightEnabled = g_prefs.getBool(AUTO_LIGHT, DEFAULT_AUTO_LIGHT_ENABLED);
  autoLightBrightness = g_prefs.getFloat(AUTO_LIGHT_BRIGHT, DEFAULT_AUTO_LIGHT_BRIGHTNESS);
  autoLightDuration = g_prefs.getUInt(AUTO_LIGHT_DURATION, DEFAULT_AUTO_LIGHT_DURATION);
}

void Settings::factoryReset() {

}

