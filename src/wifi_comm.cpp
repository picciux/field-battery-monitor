
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

#include "websocket_proto.h"

#define UPDATE_PATH "/update"
#define CAPS_PATH "/api/cap"
#define STATUS_PATH "/api/sta"
#define SETTINGS_PATH "/api/cfg"

#ifndef WIFI_SERVER_PORT
#define WIFI_SERVER_PORT 1000
#endif

#ifndef WIFI_CONNECT_TIMEOUT
#define WIFI_CONNECT_TIMEOUT 20
#endif

#define CONNECT_WAIT_COUNT  ( (WIFI_CONNECT_TIMEOUT * 1000) / 333 )

FS* filesystem = &LittleFS;
WebServer www(80);
WebSocketsServer webSocket(81);
HTTPUpdateServer updater;

WifiComm wifiComm; //WifiComm static instance

/*************************** WI-FI ****************************************/

/* AP event handlers */
void _onAPStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
}

void _onAPStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiComm.networkDisconnected();
}

/* STA event handlers */
void _onStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
}

void _onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiComm.networkDisconnected();
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
  
  uint8_t alt = 0;
  
  WiFi.setAutoConnect(false);
  WiFi.mode(WIFI_STA);
  delay(100);

  //Setting wifi hostname
  WiFi.hostname(s.getHostname());
  MDNS.begin(s.getHostname());

  if (searchAndConnectNet(s.getMainSsid(), s.getMainPsk())) {
    return true;
  } else if (searchAndConnectNet(s.getAltSsid(), s.getAltPsk())) {
    return true;
  } else {
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    
    if (WiFi.softAP(s.getHostname(), s.getApPsk())) {
       /* 
        *  if (s.ap_dont_be_default_gw) {
        *   uint8_t router = 0;
        *   if (!wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &router)) {}
        *  }
        */
      WiFi.onEvent(& _onAPStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
      WiFi.onEvent(& _onAPStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

      return true;
    }
  }
     
  return false;
}

/************************* www & websocket *************************/
int WifiComm::printStatus(char *buf, int bufsize) {
  return snprintf_P(
    buf, 
    bufsize, 
    PSTR("{\"type\":\"status\",\
       \"payload\":{\
          voltage: %f,\
          current: %f,\
          soc: %u%,\
          temp: %f,\
          rem_hours: %f,\
          auto_light_enabled: %s,\
          auto_light_brightness: %u,\
          auto_light_duration: %u,\
          cp_low_thresh: %i\
       }}"
    ),
    hardware->battery->getVoltage(),
    hardware->battery->getCurrent(),
    hardware->battery->getSoC(),
    hardware->heater->getTemperature(),
    hardware->battery->getAutonomyHours(),
    ( hardware->light->isAutoEnabled() ? "true" : "false" ),
    hardware->light->getAutoBrightness(),
    hardware->light->getAutoDuration(),
    hardware->heater->getLowThreshold()
  );
}

int WifiComm::printCaps(char *buf, int bufsize) {
#ifdef CHANNELS_4
    int nchans = 4;
#else
    int nchans = 2;
#endif

#ifndef DISABLE_LIGHT
    const char *light = "true";
#else
    const char *light = "false";
#endif

    return snprintf(
      buf, 
      bufsize, 
      "{\"type\":\"capabilities\", \"payload\":{\"channels\":%u,\"light\":%s}}",
      nchans, light
    );
}

void WifiComm::websocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {  
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      break;
    case WStype_CONNECTED:               // if a new websocket connection is established: send initial data
      char buf[200];
      printCaps(buf, 200);
      webSocket.sendTXT(num, buf);
      printStatus(buf, 200);
      webSocket.sendTXT(num, buf);   
      break;
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
        float lt = doc["temperature"] | DEFAULT_COLT_PROTECTION_LOW_THRESHOLD;
        hardware->heater->setLowThreshold(lt);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_BRIGHTNESS)) {
        float b = doc["brightness"] | 0.0;
        hardware->light->setBrightness(b);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_ENABLE)) {
        bool e = doc["enabled"] | DEFAULT_AUTO_LIGHT_ENABLED;
        hardware->light->autoEnable(e);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_BRIGHTNESS)) {
        float b = doc["brightness"] | DEFAULT_AUTO_LIGHT_BRIGHTNESS;
        hardware->light->setAutoBrightness(b);
        ret = true;
      } else if (!strcmp(action, ACTION_LIGHT_AUTO_DURATION)) {
        int s = doc["seconds"] | DEFAULT_AUTO_LIGHT_DURATION;
        hardware->light->setAutoDuration(s);
        ret = true;
      } else if (!strcmp(action, ACTION_OUTLET_POWER)) {
        int i = doc["index"] | 0;
        float p = doc["power"] | 1.0f;
        hardware->outlets[i]->setPower(p);
        ret = true;
      }

      if (ret)
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":true}");
      else
        webSocket.sendTXT(num, "{\"type\":\"result\",\"payload\":false}");
            
      break;
  }         
}

/*
void WifiComm::broadcastEvent(Hardware &hw) {
  char buf[200];
  printStatus(buf, 200);
  webSocket.broadcastTXT(buf);
}*/

void WifiComm::onHardwareChanged(HardwareEvent event, int index)
{
    char payload[128];
    int len = 0;
    Hardware *h = hardware;

    switch (event) {
        case HardwareEvent::BatteryMainData:
            len = snprintf(payload, sizeof(payload),
                "{\"event\":\"" EVENT_BATTERY "\",\"voltage\":%.2f,\"current\":%.3f,\"soc\":%.1f,\"temperature\":%.1f}",
                h->battery->getVoltage(), 
                h->battery->getCurrent(), 
                h->battery->getSoC(), 
                h->heater->getTemperature());
            break;

        case HardwareEvent::BatteryAutonomy:
            len = snprintf(payload, sizeof(payload),
                "{\"event\":\"" EVENT_BATTERY_AUTONOMY "\",\"hours\":%.1f}",
                h->battery->getAutonomyHours());
            break;

        case HardwareEvent::Heater:
            len = snprintf(payload, sizeof(payload),
                "{\"event\":\"" EVENT_CP "\",\"enabled\":%s,\"lt\":%d}",
                "true", //TODO check
                h->heater->getLowThreshold());
            break;

        case HardwareEvent::Light:
            len = snprintf(payload, sizeof(payload),
                "{\"event\":\"" EVENT_LIGHT "\",\"brightness\":%d,\"auto\":%s,\"auto_br\":%d,\"auto_dr\":%d}",
                h->light->getBrightness(), 
                (h->light->isAutoEnabled() ? "true" : "false"),
                h->light->getAutoBrightness(),
                h->light->getAutoDuration()
              );
            break;

        case HardwareEvent::Outlet:
            len = snprintf(payload, sizeof(payload),
                "{\"event\":\"" EVENT_OUTLET "\",\"index\":%d,\"power\":%d}",
                index, hardware->outlets[index]->getPower());
            break;
    }

    if (len <= 0) {
        return; // errore di formattazione — non mandare payload vuoto/corrotto
    }
    if ((size_t)len >= sizeof(payload)) {
        // troncato: snprintf ritorna la lunghezza che AVREBBE scritto, non
        // quella effettivamente scritta — questo è il modo corretto di
        // rilevare il troncamento, non basta controllare il contenuto.
        // Con payload così piccoli non dovrebbe mai succedere; se càpita,
        // è un campanello che il buffer va allargato.
    }

    webSocket.broadcastTXT(payload, len);
}

void WifiComm::sendSettings(Settings &s) {
  char buf[800];
  int len = 0;

  len = snprintf(buf, 800, 
      "{\"hostname\":\"%s\",\
       \"display_name\":\"%s\",\
       \"ap_psk\":\"%s\",\
       \"main_ssid\":\"%s\",\
       \"main_psk\":\"%s\",\
       \"alt_ssid\":\"%s\",\
       \"alt_psk\":\"%s\",\
       \"ap_no_def_gw\":%u,\
       \"version\":\"%s\"\
       }",
      s.getHostname(),
      s.getDisplayName(),
      s.getApPsk(),
      s.getMainSsid(),
      s.getMainPsk(),
      s.getAltSsid(),
      s.getAltPsk(),
      ( s.isApDefaultGWDisabled() ? "true" : "false" ),
      VERSION
  );
  if (len <= 0) return;
  if (len > sizeof(buf))
  {
    // buffer too small for content
  }

  www.send(200, "application/json", buf);
}

void WifiComm::updateSettings(Settings &s) {
  boolean reboot = false;
  
  for(uint8_t i = 0; i < www.args(); i++) {
    if (www.argName(i) == String("hostname")) {
      s.setHostname(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("display_name")) {
      s.setDisplayName(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("ap_psk")) {
      s.setApPsk(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("main_ssid")) {
      s.setMainSsid(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("main_psk")) {
      s.setMainPsk(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("alt_ssid")) {
      s.setAltSsid(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("alt_psk")) {
      s.setAltPsk(www.arg(i).c_str());
      
    } else if (www.argName(i) == String("ap_no_def_gw")) {
      s.setApDefaultGWDisabled(www.arg(i).toInt() != 0);
    } else if (www.argName(i) == String("restart")) {
      reboot = true; 
    } 
    
    //discard anything else
  }

  //here we're ok, send back modified settings
  sendSettings(s);

  //TODO implement auto reboot
  /*if (reboot)
    rebootRequest = millis();*/
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
 webSocket.begin();
 webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  wifiComm.websocketEvent(num, type, payload, lenght);
 });

 hw->battery->setChangeListener(this);
 hw->heater->setChangeListener(this);
 hw->light->setChangeListener(this);
 for (int i = 0; i < hw->getOutletsNum(); i++)
  hw->outlets[i]->setChangeListener(this);
}

void WifiComm::run() {
  www.handleClient();
  webSocket.loop();
}


