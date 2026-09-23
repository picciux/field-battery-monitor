
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
#ifdef WIFI_DEBUG_ON_WS
  #include "ws_debug.h"
#endif

#define UPDATE_PATH "/update"
#define CAPS_PATH "/api/cap"
#define STATUS_PATH "/api/sta"
#define SETTINGS_PATH "/api/cfg"

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

/* AP event handlers */
void _onAPStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
}

void _onAPStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
}

/* STA event handlers */
void _onStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiComm.restartMDNS();
}

void _onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
}

void WifiComm::networkDisconnected() {}

boolean WifiComm::searchAndConnectNet(char *ssid, char *pass) {
  byte w = 0;
  boolean found = false;

  if (strlen(ssid) == 0) return false;

  WiFi.disconnect();

  WiFi.onEvent(& _onStationConnected, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(& _onStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  
  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; i++) {
    if ( WiFi.SSID(i) == String(ssid) ) {
      //WiFi.mode(WIFI_AP_STA);
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
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.setHostname(s.getHostname());

  if (searchAndConnectNet(s.getMainSsid(), s.getMainPsk()) ||
      searchAndConnectNet(s.getAltSsid(), s.getAltPsk())) {
    _apMode = false;
    restartMDNS();
    return true;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();

  if (WiFi.softAP(s.getHostname(), s.getApPsk())) {
    WiFi.onEvent(&_onAPStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(&_onAPStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
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
    // Modalita' AP fallback: a intervalli lunghi, ritenta la rete principale.
    // Nota: usa una scansione, che blocca per 1-3s circa e puo' causare un
    // breve stallo su HTTP/websocket durante il tentativo.
    if (now - _lastFullRetry >= AP_FALLBACK_RETRY_MS) {
      _lastFullRetry = now;
      DBGLN(F("WiFi: in AP fallback, ritento la rete principale"));
      Settings *s = hardware->settings;
      if (searchAndConnectNet(s->getMainSsid(), s->getMainPsk()) ||
          searchAndConnectNet(s->getAltSsid(), s->getAltPsk())) {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _apMode = false;
        restartMDNS();
      }
    }
  }
}

void WifiComm::requestRestart() {
  DBGLN(F("Restart richiesto"));
  _restartRequested = millis();
}

/************************* www & websocket *************************/
int WifiComm::sendCaps(char *buf, int bufsize) {
  return snprintf(buf, bufsize,
    "{\"type\":\"capabilities\",\"payload\":{\"channels\":%d,\"light\":%s,\"outlets\":%d}}",
    BOARD_CHANNELS, HAS_LIGHT ? "true" : "false", OUTLET_COUNT);
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
  char payload[128];
  int len = formatEvent(event, index, payload, sizeof(payload));
  if (len > 0)
    webSocket.broadcastTXT(payload, len);
}

// Stato completo per un client appena connesso: gli stessi eventi che riceverebbe
// dal broadcast, cosi' l'interfaccia ha un unico formato da interpretare.
void WifiComm::sendInitialState(uint8_t num)
{
  char buf[128];
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

void WifiComm::websocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {  
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      break;
    case WStype_CONNECTED:  {             // if a new websocket connection is established: send initial data
      char buf[128];
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
        float b = doc["brightness"] | 0.0;
        if (hardware->light) hardware->light->setBrightness(b);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_ENABLE)) {
        bool e = doc["enabled"] | DEFAULT_AUTO_LIGHT_ENABLED;
        if (hardware->light) hardware->light->autoEnable(e);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_BRIGHTNESS)) {
        float b = doc["brightness"] | DEFAULT_AUTO_LIGHT_BRIGHTNESS;
        if (hardware->light) hardware->light->setAutoBrightness(b);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_DURATION)) {
        int s = doc["seconds"] | DEFAULT_AUTO_LIGHT_DURATION;
        if (hardware->light) hardware->light->setAutoDuration(s);
        ret = true;
      } else if (!strcmp(action, ACTION_OUTLET_POWER)) {
        int i = doc["index"] | 0;
        float p = doc["power"] | 1.0f;
        if (i >= 0 && i < hardware->getOutletsNum())
          hardware->outlets[i]->setPower(p);
        ret = true;
      } else if (!strcmp(action, ACTION_RESTART)) {
        ret = true;
        requestRestart();
      }

      if (ret)
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":true}");
      else
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":false}");
            
      break;
  }         
}


void WifiComm::sendSettings(Settings &s) {
  JsonDocument doc;
  doc["hostname"]     = s.getHostname();
  doc["display_name"] = s.getDisplayName();
  doc["main_ssid"]    = s.getMainSsid();
  doc["alt_ssid"]     = s.getAltSsid();
  doc["ap_no_def_gw"] = s.isApDefaultGWDisabled();
  doc["version"]      = VERSION;

  String out;
  serializeJson(doc, out);
  www.send(200, "application/json", out);
}

void WifiComm::updateSettings(Settings &s) {
  boolean reboot = false;
  
  for(uint8_t i = 0; i < www.args(); i++) {
    if (www.argName(i).equals("hostname")) {
      s.setHostname(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("display_name")) {
      s.setDisplayName(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("ap_psk")) {
      if (www.arg(i).length() >= 8)
        s.setApPsk(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("main_ssid")) {
      s.setMainSsid(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("main_psk")) {
      if (www.arg(i).length() >= 8)
        s.setMainPsk(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("alt_ssid")) {
      s.setAltSsid(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("alt_psk")) {
      if (www.arg(i).length() >= 8)
        s.setAltPsk(www.arg(i).c_str());
      
    } else if (www.argName(i).equals("ap_no_def_gw")) {
      s.setApDefaultGWDisabled(www.arg(i).toInt() != 0);
    } else if (www.argName(i) == String("restart")) {
      requestRestart();
    } 
    
    //discard anything else
  }

  //here we're ok, send back modified settings
  sendSettings(s);

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

#ifdef WIFI_DEBUG_ON_WIFI
 const IPAddress APbcastip = { 192, 168, 4, 255 };
 const IPAddress STAbcastip = { 255, 255, 255, 255 };

 if (WiFi.getMode() == WIFI_STA)
  getWifiDebug()->start(STAbcastip, WIFI_DEBUG_WIFI_UDP_PORT);
 else
  getWifiDebug()->start(APbcastip, WIFI_DEBUG_WIFI_UDP_PORT);
#endif

 if (!LittleFS.begin(true))
  DBGLN(F("ERROR initializing fs"));

 www.on(SETTINGS_PATH, HTTP_GET, [this, &s]() {
    this->sendSettings(s);
 });

 www.on(SETTINGS_PATH, HTTP_POST, [this, &s]() {
    this->updateSettings(s);
 });

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
#ifdef WIFI_DEBUG_ON_WS
 wsDebugSetup(&webSocket);
#endif
 webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  wifiComm.websocketEvent(num, type, payload, lenght);
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

  // Ritardo breve: lascia il tempo alla risposta HTTP/websocket di essere
  // effettivamente scritta sul socket prima del reboot.
  if (_restartRequested && (millis() - _restartRequested) >= 500) {
    ESP.restart();
  }
}


