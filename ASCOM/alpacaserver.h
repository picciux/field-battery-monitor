#pragma once
#include "esphome.h"

/***********************************************************
 *
 * Needs:
 * esphome:
     name: battery50ah
     friendly_name: Batteria 50Ah Matteo
     * includes:
     *  - alpaca_native_server.h # Estensione Alpaca Nativa
 *
 ***********************************************************/

class AlpacaNativeComponent : public Component {
public:
  void setup() override {
    // Recuperiamo l'istanza del server web attiva grazie a "web_server:" nello YAML
    auto *server = global_web_server_base->get_server();

    // ===========================================================================
    // 1. ENDPOINTS DI MANAGEMENT & HANDSHAKE (Richiesti da ASCOM Alpaca)
    // ===========================================================================

    // Verifica transazione e stato connessione per SafetyMonitor e Switch
    server->on("/api/v1/safetymonitor/0/connected", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\",\"Value\":true}");
    });
    server->on("/api/v1/switch/0/connected", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\",\"Value\":true}");
    });

    // Nome e descrizione dei dispositivi Alpaca
    server->on("/api/v1/safetymonitor/0/name", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", "{\"Value\":\"ESPHome Battery Safety Guard\",\"ErrorNumber\":0,\"ErrorMessage\":\"\"}");
    });
    server->on("/api/v1/switch/0/name", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", "{\"Value\":\"ESPHome Battery Power Station\",\"ErrorNumber\":0,\"ErrorMessage\":\"\"}");
    });

    // ===========================================================================
    // 2. LOGICA SAFETY MONITOR (Per gli allarmi di blocco in N.I.N.A. ed Ekos)
    // ===========================================================================
    server->on("/api/v1/safetymonitor/0/issafe", HTTP_GET, [](AsyncWebServerRequest *request) {
      float soc = id(soc_percentage_output).state;

      // Impostiamo una logica di sicurezza: se il SoC è NaN o sotto il 20%, scatta l'allarme
      bool is_safe = (soc > 20.0 && !std::isnan(soc));

      std::string json = "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\",\"Value\":" +
      std::string(is_safe ? "true" : "false") + "}";
      request->send(200, "application/json", json.c_str());
    });

    // ===========================================================================
    // 3. LOGICA SWITCH (Lettura Telemetria ed Controllo Carichi)
    // ===========================================================================

    // Specifichiamo ad Alpaca quanti canali virtuali espone il nostro Switch (in questo caso 5)
    server->on("/api/v1/switch/0/maxswitch", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "application/json", "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\",\"Value\":5}");
    });

    // Ritorna il valore del canale richiesto (N.I.N.A. invia il parametro ?Id=X o ?id=X)
    server->on("/api/v1/switch/0/getswitchvalue", HTTP_GET, [](AsyncWebServerRequest *request) {
      int switch_id = 0;
      if (request->hasParam("Id")) switch_id = request->getParam("Id")->value().toInt();
      else if (request->hasParam("id")) switch_id = request->getParam("id")->value().toInt();

      double val = 0.0;
      switch (switch_id) {
        case 0: val = id(channel_3_dimmer).state; break;          // Id 0: Slider PWM Uscita 1 (0-100)
        case 1: val = id(channel_4_switch).state ? 1.0 : 0.0; break; // Id 1: Stato On/Off Uscita 2 (0 o 1)
        case 2: val = id(battery_voltage).state; break;            // Id 2: Tensione (V)
        case 3: val = id(ist_current).state; break;                // Id 3: Corrente (A)
        case 4: val = id(soc_percentage_output).state; break;      // Id 4: SoC (%)
      }

      if (std::isnan(val)) val = 0.0;

      std::string json = "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\",\"Value\":" +
      std::to_string(val) + "}";
      request->send(200, "application/json", json.c_str());
    });

    // Ritorna lo stato booleano (richiesto da ASCOM per i canali on/off)
    server->on("/api/v1/switch/0/getswitch", HTTP_GET, [](AsyncWebServerRequest *request) {
      int switch_id = 0;
      if (request->hasParam("Id")) switch_id = request->getParam("Id")->value().toInt();
      else if (request->hasParam("id")) switch_id = request->getParam("id")->value().toInt();

      bool state = false;
      if (switch_id == 1) state = id(channel_4_switch).state;
      else state = (id(channel_3_dimmer).state > 0);

      std::string json = "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\",\"Value\":" +
      std::string(state ? "true" : "false") + "}";
      request->send(200, "application/json", json.c_str());
    });

    // Azione di comando: Accendi/Spegni Switch (N.I.N.A. invia una richiesta POST/PUT con parametri Id e State)
    // Nota: Accettiamo qualunque metodo HTTP per massima compatibilità con i client Alpaca
    auto handler_set_switch = [](AsyncWebServerRequest *request) {
      int switch_id = -1;
      bool state = false;

      if (request->hasParam("Id")) switch_id = request->getParam("Id")->value().toInt();
      if (request->hasParam("State")) state = request->getParam("State")->value().equalsIgnoreCase("true");

      if (switch_id == 1) { // Mappato su Uscita 2 (channel_4_switch)
        auto call = id(channel_4_switch).make_call();
        call.set_state(state);
        call.perform();
      }

      request->send(200, "application/json", "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}");
    };
    server->on("/api/v1/switch/0/setswitch", HTTP_POST, handler_set_switch);
    server->on("/api/v1/switch/0/setswitch", HTTP_PUT, handler_set_switch);

    // Azione di comando: Imposta valore Dimmer PWM (Parametri Id e Value)
    auto handler_set_value = [](AsyncWebServerRequest *request) {
      int switch_id = -1;
      double value = 0.0;

      if (request->hasParam("Id")) switch_id = request->getParam("Id")->value().toInt();
      if (request->hasParam("Value")) value = request->getParam("Value")->value().asFloat();

      if (switch_id == 0) { // Mappato su Uscita 1 Dimmer (channel_3_dimmer)
        auto call = id(channel_3_dimmer).make_call();
        call.set_value(value); // Imposta il valore dello slider 0-100
        call.perform();
      }

      request->send(200, "application/json", "{\"ClientTransactionID\":0,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}");
    };
    server->on("/api/v1/switch/0/setswitchvalue", HTTP_POST, handler_set_value);
    server->on("/api/v1/switch/0/setswitchvalue", HTTP_PUT, handler_set_value);
  }
};
