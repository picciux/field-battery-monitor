#include "AlpacaCommon.h"
#include <uri/UriBraces.h>

static uint32_t g_serverTransactionID = 0;

uint32_t AlpacaHelper::nextServerTransactionID() {
  return ++g_serverTransactionID;
}

uint32_t AlpacaHelper::getClientTransactionID(WebServer &server) {
  return (uint32_t) queryArgToInt(server, "ClientTransactionID", 0);
}

uint32_t AlpacaHelper::getClientID(WebServer &server) {
  return (uint32_t) queryArgToInt(server, "ClientID", 0);
}

int AlpacaHelper::pathArgToInt(WebServer &server, uint8_t index, int defaultValue) {
  String v = server.pathArg(index);
  if (v.length() == 0) return defaultValue;
  return v.toInt();
}

int AlpacaHelper::queryArgToInt(WebServer &server, const char *name, int defaultValue) {
  if (!server.hasArg(name)) return defaultValue;
  return server.arg(name).toInt();
}

double AlpacaHelper::queryArgToDouble(WebServer &server, const char *name, double defaultValue) {
  if (!server.hasArg(name)) return defaultValue;
  return server.arg(name).toDouble();
}

bool AlpacaHelper::queryArgToBool(WebServer &server, const char *name, bool defaultValue) {
  if (!server.hasArg(name)) return defaultValue;
  String v = server.arg(name);
  v.toLowerCase();
  return v == "true" || v == "1";
}

// ---------------------------------------------------------------------------
// Invio risposte: tutte passano da qui per garantire l'envelope Alpaca standard
// {Value, ClientTransactionID, ServerTransactionID, ErrorNumber, ErrorMessage}
// ---------------------------------------------------------------------------
static void sendJson(WebServer &server, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void AlpacaHelper::sendBool(WebServer &server, bool value, uint32_t clientTransactionID,
                             int errorNumber, const String &errorMessage) {
  JsonDocument doc;
  doc["Value"] = value;
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = errorNumber;
  doc["ErrorMessage"] = errorMessage;
  sendJson(server, doc);
}

void AlpacaHelper::sendDouble(WebServer &server, double value, uint32_t clientTransactionID,
                               int errorNumber, const String &errorMessage) {
  JsonDocument doc;
  doc["Value"] = value;
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = errorNumber;
  doc["ErrorMessage"] = errorMessage;
  sendJson(server, doc);
}

void AlpacaHelper::sendInt(WebServer &server, int value, uint32_t clientTransactionID,
                            int errorNumber, const String &errorMessage) {
  JsonDocument doc;
  doc["Value"] = value;
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = errorNumber;
  doc["ErrorMessage"] = errorMessage;
  sendJson(server, doc);
}

void AlpacaHelper::sendString(WebServer &server, const String &value, uint32_t clientTransactionID,
                               int errorNumber, const String &errorMessage) {
  JsonDocument doc;
  doc["Value"] = value;
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = errorNumber;
  doc["ErrorMessage"] = errorMessage;
  sendJson(server, doc);
}

void AlpacaHelper::sendStringArray(WebServer &server, const char *const values[], size_t count,
                                    uint32_t clientTransactionID) {
  JsonDocument doc;
  JsonArray arr = doc["Value"].to<JsonArray>();
  for (size_t i = 0; i < count; i++) arr.add(values[i]);
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = AlpacaError::OK;
  doc["ErrorMessage"] = "";
  sendJson(server, doc);
}

// Per i metodi PUT che non restituiscono un Value (es. setswitch, refresh)
void AlpacaHelper::sendEmptyOk(WebServer &server, uint32_t clientTransactionID) {
  JsonDocument doc;
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = AlpacaError::OK;
  doc["ErrorMessage"] = "";
  sendJson(server, doc);
}

void AlpacaHelper::sendError(WebServer &server, int errorNumber, const String &errorMessage,
                              uint32_t clientTransactionID) {
  JsonDocument doc;
  doc["ClientTransactionID"] = clientTransactionID;
  doc["ServerTransactionID"] = nextServerTransactionID();
  doc["ErrorNumber"] = errorNumber;
  doc["ErrorMessage"] = errorMessage;
  sendJson(server, doc);
}

// ---------------------------------------------------------------------------
// Metodi comuni ASCOM (validi per qualunque device_type, device_number = 0)
// ---------------------------------------------------------------------------
void registerCommonDeviceEndpoints(WebServer &server, const char *deviceType,
                                    const AlpacaDeviceInfo &info, bool &connectedFlag) {
  String base = String("/api/v1/") + deviceType + "/{}/";

  server.on(UriBraces(base + "connected"), HTTP_GET, [&server, &connectedFlag]() {
    AlpacaHelper::sendBool(server, connectedFlag, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "connected"), HTTP_PUT, [&server, &connectedFlag]() {
    connectedFlag = AlpacaHelper::queryArgToBool(server, "Connected", connectedFlag);
    AlpacaHelper::sendEmptyOk(server, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "description"), HTTP_GET, [&server, &info]() {
    AlpacaHelper::sendString(server, info.description, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "driverinfo"), HTTP_GET, [&server, &info]() {
    AlpacaHelper::sendString(server, info.driverInfo, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "driverversion"), HTTP_GET, [&server, &info]() {
    AlpacaHelper::sendString(server, info.driverVersion, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "interfaceversion"), HTTP_GET, [&server, &info]() {
    AlpacaHelper::sendInt(server, info.interfaceVersion, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "name"), HTTP_GET, [&server, &info]() {
    AlpacaHelper::sendString(server, info.name, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "supportedactions"), HTTP_GET, [&server]() {
    // TODO: aggiungi qui eventuali azioni custom esposte via /action
    AlpacaHelper::sendStringArray(server, nullptr, 0, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "action"), HTTP_PUT, [&server]() {
    AlpacaHelper::sendError(server, AlpacaError::ActionNotImplemented,
                             "Action not implemented", AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "commandblind"), HTTP_PUT, [&server]() {
    AlpacaHelper::sendError(server, AlpacaError::NotImplemented,
                             "CommandBlind not implemented", AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "commandbool"), HTTP_PUT, [&server]() {
    AlpacaHelper::sendError(server, AlpacaError::NotImplemented,
                             "CommandBool not implemented", AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "commandstring"), HTTP_PUT, [&server]() {
    AlpacaHelper::sendError(server, AlpacaError::NotImplemented,
                             "CommandString not implemented", AlpacaHelper::getClientTransactionID(server));
  });
}
