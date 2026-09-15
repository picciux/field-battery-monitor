#include "AlpacaManagement.h"
#include "AlpacaCommon.h"
#include "AlpacaSwitch.h"
#include <ArduinoJson.h>

#include "include_config.h"

// TODO: genera UUID stabili e unici per il tuo dispositivo (es. da MAC address)
// invece di questi placeholder statici.
static const char *SWITCH_UNIQUE_ID_BASE = "esp32-switch0-0000-0000-00000000000";
static const char *SM_UNIQUE_ID     = "esp32-sftymon-0000-0000-000000000001";

void alpacaManagementSetup(WebServer &server) {
  // /management/apiversions - versioni Alpaca API supportate da questo server
  server.on("/management/apiversions", HTTP_GET, [&server]() {
    JsonDocument doc;
    JsonArray arr = doc["Value"].to<JsonArray>();
    arr.add(1);
    doc["ClientTransactionID"] = AlpacaHelper::getClientTransactionID(server);
    doc["ServerTransactionID"] = AlpacaHelper::nextServerTransactionID();
    doc["ErrorNumber"] = AlpacaError::OK;
    doc["ErrorMessage"] = "";
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // /management/v1/description - info sul server (non sul singolo device)
  server.on("/management/v1/description", HTTP_GET, [&server]() {
    JsonDocument doc;
    JsonObject value = doc["Value"].to<JsonObject>();
    // TODO: personalizza questi campi
    value["ServerName"] = "Battery Monitor Alpaca Server";
    value["Manufacturer"] = "Matteo Piscitelli";
    value["ManufacturerVersion"] = VERSION;
    value["Location"] = "Osservatorio";
    doc["ClientTransactionID"] = AlpacaHelper::getClientTransactionID(server);
    doc["ServerTransactionID"] = AlpacaHelper::nextServerTransactionID();
    doc["ErrorNumber"] = AlpacaError::OK;
    doc["ErrorMessage"] = "";
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // /management/v1/configureddevices - elenco device esposti da questo server
  server.on("/management/v1/configureddevices", HTTP_GET, [&server]() {
    JsonDocument doc;
    JsonArray arr = doc["Value"].to<JsonArray>();

    for (int i = 0; i < getSwitchDevicesCount(); i++) {
      JsonObject sw = arr.add<JsonObject>();
      sw["DeviceName"] = getSwitchDeviceInfo(i).name;
      sw["DeviceType"] = "Switch";
      sw["DeviceNumber"] = i;
      sw["UniqueID"] = SWITCH_UNIQUE_ID_BASE + i;
    }

    JsonObject sm = arr.add<JsonObject>();
    sm["DeviceName"] = "Safety";
    sm["DeviceType"] = "SafetyMonitor";
    sm["DeviceNumber"] = 0;
    sm["UniqueID"] = SM_UNIQUE_ID;

    doc["ClientTransactionID"] = AlpacaHelper::getClientTransactionID(server);
    doc["ServerTransactionID"] = AlpacaHelper::nextServerTransactionID();
    doc["ErrorNumber"] = AlpacaError::OK;
    doc["ErrorMessage"] = "";
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });
}
