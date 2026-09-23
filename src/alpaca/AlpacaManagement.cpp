#include "AlpacaManagement.h"
#include "AlpacaCommon.h"
#include "AlpacaSwitch.h"
#include "AlpacaSafetyMonitor.h"
#include <ArduinoJson.h>

#include "include_config.h"

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

    char uid[40];

    DeviceDef swDevice = getSwitchDevice();
    JsonObject sw = arr.add<JsonObject>();
    sw["DeviceName"] = swDevice.devInfo->name;
    sw["DeviceType"] = "Switch";
    sw["DeviceNumber"] = swDevice.number;
    AlpacaHelper::makeUniqueId(uid, sizeof(uid), AlpacaDeviceType::Switch, swDevice.number);
    sw["UniqueID"] = String(uid);   // String: ArduinoJson ne fa una copia

    JsonObject sm = arr.add<JsonObject>();
    sm["DeviceName"] = getSafetyMonitorInfo().name;
    sm["DeviceType"] = "SafetyMonitor";
    sm["DeviceNumber"] = 0;
    AlpacaHelper::makeUniqueId(uid, sizeof(uid), AlpacaDeviceType::SafetyMonitor, 0);
    sm["UniqueID"] = String(uid);
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });
}
