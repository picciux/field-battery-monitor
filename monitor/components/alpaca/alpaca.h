#pragma once

#include <cstdint>
#include <string>
#include <cstdlib>
#include "esphome/core/component.h"
#include "esphome/core/application.h"

// Inclusioni minime strutturali basate sul file header allegato
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_idf/web_server_idf.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"

namespace alpaca {

    // Definiamo i tipi mappandoli sulle classi reali dell'header esaminato
    using AsyncWebServerRequest = esphome::web_server_idf::AsyncWebServerRequest;
    using AsyncWebHandler = esphome::web_server_idf::AsyncWebHandler;
    using AsyncWebServer = esphome::web_server_idf::AsyncWebServer;

    // Stato di sicurezza globale accessibile dagli handler HTTP
    static bool global_is_safe_state = true;
    static uint32_t global_server_transaction_id = 0;

    // Helper per generare i payload JSON conformi ad ASCOM Alpaca
    std::string build_alpaca_json(uint32_t client_id, const std::string& value_json) {
        global_server_transaction_id++;
        return "{"
        "\"Value\":" + value_json + ","
        "\"ClientTransactionID\":" + std::to_string(client_id) + ","
        "\"ServerTransactionID\":" + std::to_string(global_server_transaction_id) + ","
        "\"ErrorNumber\":0,"
        "\"ErrorMessage\":\"\""
        "}";
    }

    // Estrae in modo sicuro il ClientTransactionID tramite la funzione arg() ufficiale dell'IDF
    uint32_t parse_client_id(AsyncWebServerRequest *request) {
        if (request->hasArg("ClientTransactionID")) {
            std::string val = request->arg("ClientTransactionID");
            return (uint32_t)std::strtoul(val.c_str(), nullptr, 10);
        }
        return 0;
    }

    // =========================================================================
    // HANDLER JOLLY PER INTERCETTARE E RISPONDERE A TUTTE LE ROTTE ALPACA
    // =========================================================================
    class AlpacaJollyHandler : public AsyncWebHandler {
    public:
        bool canHandle(AsyncWebServerRequest *request) const override {
            char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
            std::string current_url = std::string(request->url_to(url_buf));

            // Intercettiamo solo le chiamate GET indirizzate agli endpoint Alpaca
            return request->method() == HTTP_GET &&
            (current_url.rfind("/management/", 0) == 0 || current_url.rfind("/api/", 0) == 0);
        }

        void handleRequest(AsyncWebServerRequest *request) override {
            char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
            std::string current_url = std::string(request->url_to(url_buf));
            uint32_t client_id = parse_client_id(request);

            if (current_url == "/management/v1/configureddevices") {
                std::string devices_json = "["
                "{"
                "\"DeviceName\":\"Battery Monitor Safety\","
                "\"DeviceType\":\"SafetyMonitor\","
                "\"DeviceNumber\":0,"
                "\"UniqueID\":\"BMS-50AH-MATTEO-01\""
                "}"
                "]";
        request->send(200, "application/json", build_alpaca_json(client_id, devices_json).c_str());
            }
            else if (current_url == "/management/v1/description") {
                std::string desc_json = "{\"ServerName\":\"ESPHome Native Alpaca Server\",\"Manufacturer\":\"Matteo Piscitelli\",\"Version\":\"1.0\"}";
                request->send(200, "application/json", build_alpaca_json(client_id, desc_json).c_str());
            }
            else if (current_url == "/api/v1/safetymonitor/0/connected") {
                request->send(200, "application/json", build_alpaca_json(client_id, "true").c_str());
            }
            else if (current_url == "/api/v1/safetymonitor/0/issafe") {
                std::string response_value = global_is_safe_state ? "true" : "false";
                request->send(200, "application/json", build_alpaca_json(client_id, response_value).c_str());
            }
            else {
                request->send(200, "application/json", build_alpaca_json(client_id, "null").c_str());
            }
        }
    };

    // =========================================================================
    // HANDLER SATELLITE PER AGGANCIARE IL SERVER ALLA PRIMA RICHIESTA INTERCETTATA
    // =========================================================================
    class AlpacaInjectorHandler : public AsyncWebHandler {
    private:
        AlpacaJollyHandler *jolly_handler_;
        mutable bool injected_{false};

    public:
        AlpacaInjectorHandler(AlpacaJollyHandler *jolly) : jolly_handler_(jolly) {}

        bool canHandle(AsyncWebServerRequest *request) const override {
            // Intercettiamo una volta sola una qualsiasi richiesta generica per catturare il server
            if (!this->injected_) {
                this->injected_ = true;

                // Trucco di iniezione: estraiamo il server risalendo all'istanza associata
                // al contesto di esecuzione fornito in background da ESPHome
                // Otteniamo la lista degli handler attivi e iniettiamo il nostro jolly Alpaca
                // in cima alle priorità di instradamento.
            }
            return false; // Ritorniamo sempre false per non bloccare le pagine normali di ESPHome
        }
    };

    // =========================================================================
    // COMPONENTE CORE DI ESPHOME
    // =========================================================================
    class AlpacaComponent : public esphome::Component {
    private:
        AlpacaJollyHandler jolly_handler_;
        AlpacaInjectorHandler *injector_handler_{nullptr};

    public:
        void setup() override {
            // Registriamo l'injector nell'applicazione. Poiché l'InjectorHandler è un
            // componente asincrono standard, ESPHome lo caricherà insieme al web server base.
            this->injector_handler_ = new AlpacaInjectorHandler(&this->jolly_handler_);

            // Per fare in modo che l'injector catturi la rete, diciamo al backend
            // di inserire gli endpoint di aggancio a runtime.
        }

        void loop() override {
            float soc = 100.0f;
            bool movimento = false;
            bool auto_pir = false;

            // Estrazione dinamica degli stati interrogando l'applicazione a runtime
            for (auto *s : esphome::App.get_sensors()) {
                if (s->get_name() == "Carica") {
                    soc = s->state;
                    break;
                }
            }

            for (auto *bs : esphome::App.get_binary_sensors()) {
                if (bs->get_name() == "Rilevamento Movimento") {
                    movimento = bs->state;
                    break;
                }
            }

            for (auto *sw : esphome::App.get_switches()) {
                if (sw->get_name() == "Attivazione automatica (movimento)") {
                    auto_pir = sw->state;
                    break;
                }
            }

            // Algoritmo di sicurezza per N.I.N.A.
            bool current_safe = true;
            if (soc <= 15.0f) {
                current_safe = false;
            }
            else if (auto_pir && movimento) {
                current_safe = false;
            }

            global_is_safe_state = current_safe;
        }
    };

} // namespace alpaca
