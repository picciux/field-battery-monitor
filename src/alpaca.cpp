
#include <WebServer.h>

#include "settings.h"
#include "include_config.h"

extern WebServer www;

void setup_alpaca() {
    
    // =========================================================================
 //               INIEZIONE ROTTE REST ASCOM ALPACA (NATIVE)
 // =========================================================================

 // 1. Endpoint di Management: Configured Devices
 www.on("/management/v1/configureddevices", HTTP_GET, []() {
  extern Settings settings;  
  uint32_t client_id = www.hasArg("ClientTransactionID") ? www.arg("ClientTransactionID").toInt() : 0;
   String json = "{\"Value\":[{\"DeviceName\":\"" + 
      String(settings.display_name) + 
      "\",\"DeviceType\":\"SafetyMonitor\",\"DeviceNumber\":0,\"UniqueID\":\"" + String(settings.hostname) + "\"}],";
   json += "\"ClientTransactionID\":" + String(client_id) + ",\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}";
   www.send(200, "application/json", json);
 });

 // 2. Endpoint di Management: Device Description
 www.on("/management/v1/description", HTTP_GET, []() {
   uint32_t client_id = www.hasArg("ClientTransactionID") ? www.arg("ClientTransactionID").toInt() : 0;
   String json = "{\"Value\":{\"ServerName\":\"Battery Monitor Alpaca Server\",\"Manufacturer\":\"Matteo Piscitelli\",\"" + String(VERSION) + "\":\"1.0\"},";
   json += "\"ClientTransactionID\":" + String(client_id) + ",\"ServerTransactionID\":2,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}";
   www.send(200, "application/json", json);
 });

 // 3. API Safety Monitor: Connected
 www.on("/api/v1/safetymonitor/0/connected", HTTP_GET, []() {
   uint32_t client_id = www.hasArg("ClientTransactionID") ? www.arg("ClientTransactionID").toInt() : 0;
   String json = "{\"Value\":true,\"ClientTransactionID\":" + String(client_id) + ",\"ServerTransactionID\":3,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}";
   www.send(200, "application/json", json);
 });

 // 4. API Safety Monitor: IsSafe (Il cuore del controllo per N.I.N.A.)
 www.on("/api/v1/safetymonitor/0/issafe", HTTP_GET, []() {
   uint32_t client_id = www.hasArg("ClientTransactionID") ? www.arg("ClientTransactionID").toInt() : 0;

   // Recuperiamo le variabili globali calcolate dai tuoi sensori C++
   extern float current_soc;

   bool is_safe = true;
   if (current_soc <= 15.0f ) {
     is_safe = false; // Batteria scarica o intruso vicino al telescopio -> Ferma tutto!
   }

   String json = "{\"Value\":" + String(is_safe ? "true" : "false") + ",";
   json += "\"ClientTransactionID\":" + String(client_id) + ",\"ServerTransactionID\":4,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}";
   www.send(200, "application/json", json);
 });

}