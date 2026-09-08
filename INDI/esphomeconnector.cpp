#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <curl/curl.h>
#include <indi_json.h>

class ESPHomeINDIConnector {
private:
    std::string base_url;       // Es: "http://centralina.local"
    std::thread sse_thread;
    std::atomic<bool> is_running{false};

    // Callback di libcurl per il flusso SSE (/events)
    static size_t sse_callback(void* contents, size_t size, size_t nmemb, void* user_data) {
        size_t total_size = size * nmemb;
        std::string raw_data(static_cast<char*>(contents), total_size);

        ESPHomeINDIConnector* instance = static_cast<ESPHomeINDIConnector*>(user_data);
        instance->handle_sse_message(raw_data);

        // Se decidiamo di fermare il driver, ritornando 0 libcurl chiude la connessione
        if (!instance->is_running) return 0;

        return total_size;
    }

    // Processa i messaggi in arrivo da ESPHome
    void handle_sse_message(const std::string& raw_message) {
        // I messaggi di ESPHome arrivano solitamente preceduti da "data: "
        // e terminano con un doppio newline (\n\n) a causa dello standard SSE.

        size_t data_pos = raw_message.find("data:");
        if (data_pos == std::string::npos) return;

        // Estraiamo solo la stringa JSON pura eliminando il prefisso "data:" e gli spazi/invii
        std::string json_str = raw_message.substr(data_pos + 5);

        try {
            // Parsing nativo con la libreria nlohmann integrata in INDI
            auto j = nlohmann::json::parse(json_str);

            // ESPHome v3 nel flusso eventi restituisce sempre un oggetto con "id", "value" e "state"
            if (j.contains("id") && j.contains("value")) {
                std::string entity_id = j["id"].get<std::string>();

                // Gestione dei NUMBER
                if (entity_id.rfind("number-", 0) == 0) {
                    double current_value = j["value"].get<double>();

                    // TODO: Cerca la tua proprietà INDI basandoti sul nome o sull'id dell'entità
                    // Esempio ipotetico di aggiornamento proprietà INDI:
                    // MyTargetNumber[0].value = current_value;
                    // IDSetNumber(&MyTargetNumberProp, nullptr);
                }
                // Gestione dei SENSORI (Lettura)
                else if (entity_id.rfind("sensor-", 0) == 0) {
                    double sensor_val = j["value"].get<double>();
                    // Aggiorna la proprietà numerica di Ekos (es. Temperatura, Umidità...)
                }
                // Gestione degli SWITCH / LIGHT
                else if (entity_id.rfind("switch-", 0) == 0 || entity_id.rfind("light-", 0) == 0) {
                    std::string state_str = j["state"].get<std::string>(); // Spesso è "ON" o "OFF"
                    // ISSwitch s = (state_str == "ON") ? ISS_ON : ISS_OFF;
                    // MySwitch[0].s = s;
                    // IDSetSwitch(&MySwitchProp, nullptr);
                }
            }
        }
        catch (const nlohmann::json::parse_error& e) {
            // Ignoriamo i messaggi parziali o i ping di keep-alive vuoti inviati da ESPHome
            // (ESPHome invia periodicamente righe vuote o commenti ": ping" per tenere vivo il socket)
        }
    }

    // Ciclo del thread permanente per SSE
    void listen_sse_loop() {
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string url = base_url + "/events";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Imposta la callback che riceverà i dati man mano che arrivano
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

            // Disattiva il timeout (la connessione DEVE essere perpetua)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

            // Avvia la richiesta (questa chiamata è bloccante all'interno del thread)
            CURLcode res = curl_easy_perform(curl);

            curl_easy_cleanup(curl);
        }
    }

public:
    ESPHomeINDIConnector(const std::string& ip_or_host) {
        base_url = ip_or_host;
    }

    ~ESPHomeINDIConnector() {
        disconnect();
    }

    // Da chiamare nel metodo Connect() del tuo driver INDI
    bool connect() {
        if (is_running) return true;

        is_running = true;
        // Avvia il canale permanente in background
        sse_thread = std::thread(&ESPHomeINDIConnector::listen_sse_loop, this);
        return true;
    }

    // Da chiamare nel metodo Disconnect() del tuo driver INDI
    void disconnect() {
        if (is_running) {
            is_running = false;
            if (sse_thread.joinable()) {
                sse_thread.join(); // Attende la chiusura pulita del canale SSE
            }
        }
    }

    // Canale 2: Invia un comando POST "usa e getta" (Chiamata parallela)
    // Da invocare dentro i tuoi metodi ISNewNumber, ISNewSwitch, ecc.
    bool send_post_command(const std::string& domain, const std::string& entity_name, const std::string& action, const std::string& query_params = "") {
        CURL* curl = curl_easy_init();
        bool success = false;

        if (curl) {
            // Costruisce l'URL: http://nodo.local
            std::string url = base_url + "/" + domain + "/" + entity_name + "/" + action;
            if (!query_params.empty()) {
                url += "?" + query_params;
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Forziamo il metodo POST. ESPHome accetta anche un payload vuoto se l'azione è nell'URL
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);

            // Imposta un timeout breve (es. 5 secondi) per non bloccare Ekos in caso di disconnessione
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                if (response_code == 200) {
                    success = true;
                }
            }
            curl_easy_cleanup(curl);
        }
        return success;
    }
};

