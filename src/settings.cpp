
#include "include_config.h"


#include <EEPROM.h>
#include "settings.h"

//EEPROM positions
#define PRESENCE_BYTE_POS         0
#define PRESENT_BYTE_VALUE        100
#define NOT_PRESENT_BYTE_VALUE    255

#define SETTINGS_POS              (PRESENCE_BYTE_POS + 1)
#define SETTINGS_LEN              (sizeof (Settings))

#define isPresent() ( EEPROM.read(PRESENCE_BYTE_POS) == PRESENT_BYTE_VALUE )
#define setPresence() ( EEPROM.write(PRESENCE_BYTE_POS, PRESENT_BYTE_VALUE) )

void initSettings() {
  EEPROM.begin(512);
}

void loadSettings(Settings *settings) {  
  if (isPresent()) {
    Settings tmp;
    EEPROM.get(SETTINGS_POS, tmp);
    memcpy(settings, &tmp, SETTINGS_LEN);
  } else {
    //fill defaults
    strcpy(settings->hostname, DEFAULT_HOSTNAME);
    strcpy(settings->display_name, DEFAULT_DISPLAY_NAME);
    strcpy(settings->ap_psk, DEFAULT_AP_PSK);
    strcpy(settings->main_ssid, DEFAULT_MAIN_SSID);
    strcpy(settings->main_psk, DEFAULT_MAIN_PSK);
    strcpy(settings->alt_ssid, DEFAULT_ALT_SSID);
    strcpy(settings->alt_psk, DEFAULT_ALT_PSK);
    settings->ap_dont_be_default_gw = DEFAULT_AP_DONT_BE_DEF_GW;
  }
}

void _storeSettings(Settings *settings) {
  Settings tmp;
  memcpy(&tmp, settings, SETTINGS_LEN);
  EEPROM.put(SETTINGS_POS, tmp);
  setPresence();
}

void storeSettings(Settings *settings) {
  _storeSettings(settings);
  EEPROM.commit();
}

void factoryReset() {
  EEPROM.write(PRESENCE_BYTE_POS, 0);
  EEPROM.commit();
}

