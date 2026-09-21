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

void AlpacaHelper::makeUniqueId(char *buf, size_t size, uint16_t deviceType, uint16_t deviceNumber) {
  // Formato UUID 8-4-4-4-12. Deterministico: stesso device -> stesso ID a ogni
  // boot e a ogni cambio di IP. L'ultimo gruppo e' il MAC (eFuse).
  uint64_t mac = ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL;
  snprintf(buf, size, "%08x-%04x-%04x-%04x-%012llx",
           0xBA77E41Cu, (unsigned) deviceType, (unsigned) deviceNumber, 0u,
           (unsigned long long) mac);
}

// ---------------------------------------------------------------------------
// Metodi comuni ASCOM (validi per qualunque device_type, device_number = 0)
// ---------------------------------------------------------------------------
static bool resolveDevice(WebServer &server, AlpacaDeviceResolver resolver,
                          AlpacaDeviceRef &ref, uint32_t &ctid) {
  ctid = AlpacaHelper::getClientTransactionID(server);
  ref = resolver(AlpacaHelper::pathArgToInt(server, 0, -1));
  if (!ref.info) {
    AlpacaHelper::sendError(server, AlpacaError::InvalidValue,
                             "Device number out of range", ctid);
    return false;
  }
  return true;
}

static void registerStringProperty(WebServer &server, const String &path,
                                   AlpacaDeviceResolver resolver,
                                   const char *AlpacaDeviceInfo::*field) {
  server.on(UriBraces(path), HTTP_GET, [&server, resolver, field]() {
    AlpacaDeviceRef ref; uint32_t ctid;
    if (!resolveDevice(server, resolver, ref, ctid)) return;
    AlpacaHelper::sendString(server, ref.info->*field, ctid);
  });
}

void registerCommonDeviceEndpoints(WebServer &server, const char *deviceType,
                                    AlpacaDeviceResolver resolver) {
  const String base = String("/api/v1/") + deviceType + "/{}/";

  server.on(UriBraces(base + "connected"), HTTP_GET, [&server, resolver]() {
    AlpacaDeviceRef ref; uint32_t ctid;
    if (!resolveDevice(server, resolver, ref, ctid)) return;
    AlpacaHelper::sendBool(server, *ref.connected, ctid);
  });

  server.on(UriBraces(base + "connected"), HTTP_PUT, [&server, resolver]() {
    AlpacaDeviceRef ref; uint32_t ctid;
    if (!resolveDevice(server, resolver, ref, ctid)) return;
    *ref.connected = AlpacaHelper::queryArgToBool(server, "Connected", *ref.connected);
    AlpacaHelper::sendEmptyOk(server, ctid);
  });

  registerStringProperty(server, base + "description",   resolver, &AlpacaDeviceInfo::description);
  registerStringProperty(server, base + "driverinfo",    resolver, &AlpacaDeviceInfo::driverInfo);
  registerStringProperty(server, base + "driverversion", resolver, &AlpacaDeviceInfo::driverVersion);
  registerStringProperty(server, base + "name",          resolver, &AlpacaDeviceInfo::name);

  server.on(UriBraces(base + "interfaceversion"), HTTP_GET, [&server, resolver]() {
    AlpacaDeviceRef ref; uint32_t ctid;
    if (!resolveDevice(server, resolver, ref, ctid)) return;
    AlpacaHelper::sendInt(server, ref.info->interfaceVersion, ctid);
  });

  // supportedactions, action, commandblind, commandbool, commandstring:
  // invariati rispetto a ora (non dipendono dal device)
}