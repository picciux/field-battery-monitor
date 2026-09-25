
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "include_config.h"

#include "wifi_comm.h"
#include "hardware.h"
//#include "alpaca.h"
#include "alpaca/AlpacaManagement.h"
#include "alpaca/AlpacaSwitch.h"
#include "alpaca/AlpacaObservingConditions.h"
#include "alpaca/AlpacaSafetyMonitor.h"
#include "alpaca/AlpacaDiscovery.h"

#include "websocket_proto.h"
#include "board.h"
#ifdef DEBUG_ON_WS
  #include "ws_debug.h"
#endif

#define UPDATE_PATH "/update"

#ifndef WWW_PORT
#define WWW_PORT 80
#endif

#ifndef WIFI_CONNECT_TIMEOUT
#define WIFI_CONNECT_TIMEOUT 20
#endif

#define CONNECT_WAIT_COUNT  ( (WIFI_CONNECT_TIMEOUT * 1000) / 333 )

#define STA_RECONNECT_CHECK_MS     10000    // ogni 10s, se la STA e' caduta, richiama reconnect()
#define AP_FALLBACK_RETRY_MS      300000    // ogni 5 min, se in AP fallback, ritenta la rete principale

FS* filesystem = &LittleFS;
WebServer www(WWW_PORT);
WebSocketsServer webSocket(81);
HTTPUpdateServer updater;

WifiComm wifiComm; //WifiComm static instance

/*************************** WI-FI ****************************************/

static volatile bool g_mdnsRestartPending = false;
static void _onStationGotIp(WiFiEvent_t, WiFiEventInfo_t) { g_mdnsRestartPending = true; }

/* STA event handlers 
void _onStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiComm.restartMDNS();
}*/

void WifiComm::networkDisconnected() {}

void WifiComm::apRetryStep(unsigned long now) {
  Settings *s = hardware->settings;
  switch (_retryState) {
    case RetryState::Idle:
      if (now - _lastFullRetry < AP_FALLBACK_RETRY_MS) return;
      _lastFullRetry = now;
      if (WiFi.softAPgetStationNum() > 0) return;   // qualcuno usa l'AP: non disturbare
      WiFi.scanNetworks(true);                      // asincrona
      _retryStart = now;
      _retryState = RetryState::Scanning;
      break;

    case RetryState::Scanning: {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) {
        if (now - _retryStart > 15000) { WiFi.scanDelete(); _retryState = RetryState::Idle; }
        return;
      }
      if (n < 0) { _retryState = RetryState::Idle; return; }
      bool mainSeen = false, altSeen = false;
      for (int i = 0; i < n; i++) {
        String f = WiFi.SSID(i);
        if (f == s->getMainSsid()) mainSeen = true;
        if (f == s->getAltSsid())  altSeen = true;
      }
      WiFi.scanDelete();
      const char *ssid = nullptr, *psk = nullptr;
      if (mainSeen && strlen(s->getMainSsid())) { ssid = s->getMainSsid(); psk = s->getMainPsk(); }
      else if (altSeen && strlen(s->getAltSsid())) { ssid = s->getAltSsid(); psk = s->getAltPsk(); }
      if (!ssid) { _retryState = RetryState::Idle; return; }
      if (strlen(psk)) WiFi.begin(ssid, psk); else WiFi.begin(ssid);   // AP_STA: l'AP resta su
      _retryStart = now;
      _retryState = RetryState::Connecting;
      break;
    }

    case RetryState::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _apMode = false;
        g_mdnsRestartPending = true;
        _retryState = RetryState::Idle;
      } else if (now - _retryStart >= (unsigned long) WIFI_CONNECT_TIMEOUT * 1000UL) {
        WiFi.disconnect(false);                     // resta in AP_STA
        _retryState = RetryState::Idle;
      }
      break;
  }
}

boolean WifiComm::searchAndConnectNet(char *ssid, char *pass, const char *hostname) {
  byte w = 0;
  boolean found = false;

  if (strlen(ssid) == 0) return false;

  WiFi.disconnect();
  
  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; i++) {
    if ( WiFi.SSID(i) == String(ssid) ) {
      //WiFi.mode(WIFI_AP_STA);
      WiFi.setHostname(hostname);
      WiFi.mode(WIFI_STA);
      if (strlen(pass) > 0)
        WiFi.begin(ssid, pass);
      else
        WiFi.begin(ssid);

      found = true;
      break; //loop
    }
  }

  if (found) {
    while((WiFi.status() != WL_CONNECTED) && (w < CONNECT_WAIT_COUNT)) {
      w++;
      delay(333);
    }

    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
  }

  return false;
}

boolean WifiComm::wifiStart(Settings &s) {
  
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.persistent(false);
  delay(100);

  WiFi.setAutoConnect(false);

  WiFi.setHostname(s.getHostname());
  WiFi.mode(WIFI_STA);

  if (searchAndConnectNet(s.getMainSsid(), s.getMainPsk(), s.getHostname()) ||
      searchAndConnectNet(s.getAltSsid(), s.getAltPsk(), s.getHostname())) {
    _apMode = false;
    g_mdnsRestartPending = true;
    return true;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();

  if (WiFi.softAP(s.getHostname(), s.getApPsk())) {
    _apMode = true;
    restartMDNS();
    return true;
  }

  return false;
}

void WifiComm::restartMDNS() {
  // Il responder mDNS su ESP32 puo' restare in uno stato incoerente dopo un
  // cambio di interfaccia (STA->AP o riconnessione): end()+begin() lo rifonda.
  MDNS.end();
  if (!MDNS.begin(hardware->settings->getHostname()))
    DBGLN(F("ERROR (re)starting mDNS"));
}

void WifiComm::reconnectCheck(unsigned long now) {
  if (!_apMode) {
    // Modalita' STA: se la connessione e' caduta, richiama reconnect() (non
    // bloccante, riusa le credenziali correnti) a intervalli regolari.
    if (WiFi.status() != WL_CONNECTED) {
      if (now - _lastReconnectCheck >= STA_RECONNECT_CHECK_MS) {
        _lastReconnectCheck = now;
        DBGLN(F("WiFi: connessione persa, tento reconnect"));
        WiFi.reconnect();
      }
    }
  } else {
    apRetryStep(now);
  }
}

void WifiComm::requestRestart() {
  DBGLN(F("Restart richiesto"));
  _restartRequested = millis();
}

/************************* www & websocket *************************/
int WifiComm::sendCaps(char *buf, int bufsize) {
  return snprintf(buf, bufsize,
    "{\"type\":\"capabilities\",\"payload\":{\"channels\":%d,\"light\":%s,\"outlets\":%d,\
    \"light_auto_br_min_pct\":%u,\"light_auto_dr_min\":%u,\"light_auto_dr_max\":%u,\
    \"cp_lt_min\":%d,\"cp_lt_max\":%d,\"fs_size\":%u}}",
    BOARD_CHANNELS, HAS_LIGHT ? "true" : "false", OUTLET_COUNT, 
    LIGHT_AUTO_BRIGHTNESS_MIN_PCT, LIGHT_AUTO_DURATION_MIN_S, LIGHT_AUTO_DURATION_MAX_S, 
    CP_LOW_THRESHOLD_MIN_C, CP_LOW_THRESHOLD_MAX_C, ESP.getFlashChipSize());
}

// Formatta un evento in JSON nel buffer. Ritorna la lunghezza scritta, oppure 0
// se l'evento non e' applicabile a questa variante hardware o se il buffer
// e' troppo piccolo (snprintf ritorna la lunghezza che AVREBBE scritto:
// len >= size significa troncamento).
int WifiComm::formatEvent(HardwareEvent event, int index, char *buf, size_t size)
{
  Hardware *h = hardware;
  int len = -1;

  switch (event) {
    case HardwareEvent::BatteryMainData: {
      char temp[12];
      if (h->heater->isTemperatureValid())
        snprintf(temp, sizeof(temp), "%.1f", h->heater->getTemperature());
      else
        strlcpy(temp, "null", sizeof(temp));

      char volt[12], curr[12];
      if (h->battery->isSensorValid()) {
        snprintf(volt, sizeof(volt), "%.2f", h->battery->getVoltage());
        snprintf(curr, sizeof(curr), "%.3f", h->battery->getCurrent());
      } else {
        strlcpy(volt, "null", sizeof(volt));
        strlcpy(curr, "null", sizeof(curr));
      }

      len = snprintf(buf, size,
        "{\"event\":\"" EVENT_BATTERY "\",\"voltage\":%s,\"current\":%s,"
        "\"soc\":%.1f,\"temperature\":%s,\"battery_sensor_ok\":%s}",
        volt, curr, h->battery->getSoC(), temp,
        h->battery->isSensorValid() ? "true" : "false");
      break;
    }

    case HardwareEvent::BatteryAutonomy:
      len = snprintf(buf, size,
        "{\"event\":\"" EVENT_BATTERY_AUTONOMY "\",\"hours\":%.1f}",
        h->battery->getAutonomyHours());
      break;

    case HardwareEvent::Safety:
      len = snprintf(buf, size,
        "{\"event\":\"" EVENT_SAFETY "\",\"is_safe\":%s}",
        h->battery->isSafe() ? "true" : "false");
      break;

    case HardwareEvent::Heater:
      len = snprintf(buf, size,
        "{\"event\":\"" EVENT_CP "\",\"lt\":%.1f}",
        h->heater->getLowThreshold());
      break;

    case HardwareEvent::Light:
      if (!h->light) return 0;   // variante senza luce
      len = snprintf(buf, size,
        "{\"event\":\"" EVENT_LIGHT "\",\"brightness\":%.2f,\"auto\":%s,"
        "\"auto_br\":%.2f,\"auto_dr\":%d}",
        h->light->getBrightness(),
        h->light->isAutoEnabled() ? "true" : "false",
        h->light->getAutoBrightness(),
        h->light->getAutoDuration());
      break;

    case HardwareEvent::Outlet:
      if (index < 0 || index >= h->getOutletsNum()) return 0;
      len = snprintf(buf, size,
        "{\"event\":\"" EVENT_OUTLET "\",\"index\":%d,\"power\":%.2f}",
        index, h->outlets[index]->getPower());
      break;
  
  }

  if (len < 0 || (size_t) len >= size) return 0;   // errore o troncamento
  return len;
}

void WifiComm::onHardwareChanged(HardwareEvent event, int index)
{
  char payload[256];
  int len = formatEvent(event, index, payload, sizeof(payload));
  if (len > 0)
    webSocket.broadcastTXT(payload, len);
}

// Stato completo per un client appena connesso: gli stessi eventi che riceverebbe
// dal broadcast, cosi' l'interfaccia ha un unico formato da interpretare.
void WifiComm::sendInitialState(uint8_t num)
{
  char buf[256];
  auto send = [&](HardwareEvent e, int idx) {
    int len = formatEvent(e, idx, buf, sizeof(buf));
    if (len > 0) webSocket.sendTXT(num, buf, len);
  };

  send(HardwareEvent::BatteryMainData, 0);
  send(HardwareEvent::BatteryAutonomy, 0);
  send(HardwareEvent::Safety, 0);
  send(HardwareEvent::Heater, 0);
  send(HardwareEvent::Light, 0);
  for (int i = 0; i < hardware->getOutletsNum(); i++)
    send(HardwareEvent::Outlet, i);
}

void WifiComm::websocketEvent(Settings &s, uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {  
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      break;
    case WStype_CONNECTED:  {             // if a new websocket connection is established: send initial data
      char buf[256];
      int len = sendCaps(buf, sizeof(buf));
      if (len > 0) webSocket.sendTXT(num, buf, len);
      sendInitialState(num);
      break;
    }
    case WStype_TEXT:                     // if new text data is received
      bool ret = false;
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, lenght);
      if (error) {
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":false}");
        return;
      }
      const char *action = doc["action"] | "unknown";
      if (!strcmp(action, ACTION_BATTERY_SOC_RESET)) {
        hardware->battery->reset();
        ret = true;
      } else if (!strcmp(action, ACTION_CP_SET_LT)) {
        float lt = doc["temperature"] | DEFAULT_COLD_PROTECTION_LOW_THRESHOLD;
        hardware->heater->setLowThreshold(lt);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_BRIGHTNESS)) {
        if (HAS_LIGHT) {
          float b = doc["brightness"] | 0.0;
          if (hardware->light) hardware->light->setBrightness(b);
          ret = true;
        } else {
          ret = false;
        }
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_ENABLE)) {
        if (HAS_LIGHT) {
          bool e = doc["enabled"] | DEFAULT_AUTO_LIGHT_ENABLED;
          if (hardware->light) hardware->light->autoEnable(e);
          ret = true;
        } else {
          ret = false;
        }
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_BRIGHTNESS)) {
        if (HAS_LIGHT) {
          float b = doc["brightness"] | DEFAULT_AUTO_LIGHT_BRIGHTNESS;
          if (hardware->light) hardware->light->setAutoBrightness(b);
          ret = true;
        } else {
          ret = false;
        }
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_DURATION)) {
        if (HAS_LIGHT) {
          int s = doc["seconds"] | DEFAULT_AUTO_LIGHT_DURATION;
          if (hardware->light) hardware->light->setAutoDuration(s);
          ret = true;
        } else {
          ret = false;
        }
      } else if (!strcmp(action, ACTION_OUTLET_POWER)) {
        int i = doc["index"] | 0;
        float p = doc["power"] | 1.0f;
        if (i >= 0 && i < hardware->getOutletsNum()) {
          hardware->outlets[i]->setPower(p);
          ret = true;
        } else {
          ret = false;
        }
      } else if (!strcmp(action, ACTION_RESTART)) {
        ret = true;
        requestRestart();
      } else if (!strcmp(action, ACTION_GET_SETTINGS)) {
        sendSettings(s, num);
        return;
      } else if (!strcmp(action, ACTION_UPDATE_SETTINGS)) {
        updateSettings(s, num, doc["payload"]);
        return;
      }

      if (ret)
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":true}");
      else
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":false}");
            
      break;
  }         
}

void WifiComm::sendSettings(Settings &s, uint8_t num) {
  JsonDocument doc;
  doc["type"] = "settings";
  
  JsonObject pld = doc["payload"].to<JsonObject>();
  pld["hostname"]     = s.getHostname();
  pld["display_name"] = s.getDisplayName();
  pld["main_ssid"]    = s.getMainSsid();
  pld["alt_ssid"]     = s.getAltSsid();
  pld["ap_no_def_gw"] = s.isApDefaultGWDisabled();
  pld["version"]      = VERSION;
 
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(num, out);
}

void WifiComm::updateSettings(Settings &s, uint8_t num, JsonVariantConst p) {
  bool factoryReset = false;
  bool factoryResetConfirm = false;

  const char *val = nullptr;

  // Campi senza vincolo di lunghezza minima

  if (p["hostname"].is<const char *>()) {
    val = p["hostname"];
    if (strlen(val) > 0)
      s.setHostname(val);
  }
  if (p["display_name"].is<const char *>()) {
    val = p["display_name"];
    if (strlen(val) > 0)
      s.setDisplayName(val);
  }
  if (p["main_ssid"].is<const char *>()) {
    val = p["main_ssid"];
    if (val && strlen(val) > 0)
      s.setMainSsid(val);
  }
  if (p["alt_ssid"].is<const char *>()) {
    val = p["alt_ssid"];
    if (val && strlen(val) > 0)
      s.setAltSsid(val);
  }

  // PSK: vincolo minimo 8 caratteri (requisito WPA2)
  if (p["ap_psk"].is<const char *>()) {
    val = p["ap_psk"];
    if (val && strlen(val) >= 8)
      s.setApPsk(val);
  }
  if (p["main_psk"].is<const char *>()) {
    val = p["main_psk"];
    if (val && strlen(val) >= 8)
      s.setMainPsk(val);
  }
  if (p["alt_psk"].is<const char *>()) {
    val = p["alt_psk"];
    if (val && strlen(val) >= 8)
      s.setAltPsk(val);
  }

  if (p["ap_no_def_gw"].is<bool>() || p["ap_no_def_gw"].is<int>()) {
    s.setApDefaultGWDisabled(p["ap_no_def_gw"].as<bool>());
  }

  if (p["factory_reset"].is<bool>() && p["factory_reset"].as<bool>()) {
    factoryReset = true;
  }

  if (p["factory_reset_confirm"].is<const char*>()) {
    const char *confirm = p["factory_reset_confirm"];
    if (!strcmp(confirm, "CONFIRM FACTORY RESET")) {
      factoryResetConfirm = true;
    }
  }

  if (factoryReset && factoryResetConfirm)
    s.factoryReset();

  // qui siamo ok, rimandiamo indietro le impostazioni modificate
  sendSettings(s, num);

  if (factoryReset && factoryResetConfirm)
    requestRestart();
}

bool WifiComm::sendFile(String path) {
  if (path.endsWith("/")) {
    path += "index.html";
  }
  
  String contentType = "text/plain";
  if (path.endsWith(".htm")) {
    contentType =  "text/html";
  } else if (path.endsWith(".html")) {
    contentType =  "text/html";
  } else if (path.endsWith(".css")) {
    contentType =  "text/css";
  } else if (path.endsWith(".js")) {
    contentType =  "application/javascript";
  } else if (path.endsWith(".gif")) {
    contentType =  "image/gif";
  }

  String pathWithGz = path + ".gz";
  if (filesystem->exists(pathWithGz)) {
    path += ".gz";
  } else if (! filesystem->exists(path)) {
    return false;
  }

  File file = filesystem->open(path, "r");
  www.streamFile(file, contentType);
  file.close();
  return true;
}

/************************* setup *************************/
void WifiComm::setup(Settings &s, Hardware *hw) {
  this->hardware = hw;
  if (! wifiStart(s)) return;

  WiFi.onEvent(_onStationGotIp, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  if (!LittleFS.begin(true))
    DBGLN(F("ERROR initializing fs"));

  //serve files from SPIFFS or not found
  www.onNotFound([this]() {
    if (!this->sendFile(www.uri())) {
      www.send(404, "text/plain", "Uri not found " + www.uri());
    }
  });

  alpacaManagementSetup(www);
  alpacaSwitchSetup(www, hw);
  //alpacaObservingConditionsSetup(www);
  alpacaSafetyMonitorSetup(www, hw);

  //setup_alpaca(s, www, hw); 

  updater.setup(&www, UPDATE_PATH);

  www.begin();
  alpacaDiscoverySetup(WWW_PORT);
  webSocket.begin();
#ifdef DEBUG_ON_WS
  wsDebugSetup(&webSocket);
#endif
  webSocket.onEvent([this, &s](uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    wifiComm.websocketEvent(s, num, type, payload, lenght);
  });

  hw->battery->setChangeListener(this);
  hw->heater->setChangeListener(this);
  if (hw->light)
    hw->light->setChangeListener(this);
  for (int i = 0; i < hw->getOutletsNum(); i++)
    hw->outlets[i]->setChangeListener(this);
}

void WifiComm::run() {
  www.handleClient();
  webSocket.loop();
  alpacaDiscoveryRun();
  reconnectCheck(millis());

  if (g_mdnsRestartPending) { g_mdnsRestartPending = false; restartMDNS(); }

  // Ritardo breve: lascia il tempo alla risposta HTTP/websocket di essere
  // effettivamente scritta sul socket prima del reboot.
  if (_restartRequested && (millis() - _restartRequested) >= 500) {
    ESP.restart();
  }
}


