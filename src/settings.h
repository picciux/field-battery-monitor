#ifndef SETTINGS_H
#define SETTINGS_H

#define HOSTNAME_LEN          31  //30 chars + null-term
#define SSID_LEN              33  //32 chars + null-term
#define PSK_LEN               64  //63 chars + null-term

typedef struct {
  /* hostname: will be network SSID when acting as an AP */
  char hostname[HOSTNAME_LEN];
  char display_name[HOSTNAME_LEN];

  /* Network password when acting as an AP */
  char ap_psk[PSK_LEN];

  /* Preferred network SSID and password to connect to */
  char main_ssid[SSID_LEN];
  char main_psk[PSK_LEN];

  /* Alternative network SSID and password to connect to if main is not in range*/ 
  char alt_ssid[SSID_LEN];
  char alt_psk[PSK_LEN];

  uint8_t auto_light_enabled;
  uint8_t auto_light_brightness;
  uint8_t auto_light_duration;

  uint8_t cold_protection;
  uint8_t cp_low_thresh;
  uint8_t cp_high_thresh;

  /* Don't be default gateway for clients when acting as an AP */
  uint8_t ap_dont_be_default_gw;
} Settings;

void initSettings();
void loadSettings(Settings *settings);
void storeSettings(Settings *settings);
void factoryReset();

#endif //SETTINGS_H
