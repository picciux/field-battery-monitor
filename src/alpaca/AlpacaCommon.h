#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Codici di errore standard ASCOM Alpaca (ASCOM.Exceptions)
// https://ascom-standards.org/Developer/AlpacaHttpApi.html
// ---------------------------------------------------------------------------
namespace AlpacaError {
  constexpr int OK                   = 0;
  constexpr int NotImplemented       = 0x400; // 1024
  constexpr int InvalidValue         = 0x401; // 1025
  constexpr int ValueNotSet          = 0x402; // 1026
  constexpr int NotConnected         = 0x407; // 1031
  constexpr int InvalidOperation     = 0x40B; // 1035
  constexpr int ActionNotImplemented = 0x40C; // 1036
}

// ---------------------------------------------------------------------------
// Info statiche di un device Alpaca (usate per i metodi "comuni" ASCOM)
// ---------------------------------------------------------------------------
struct AlpacaDeviceInfo {
  const char* name;
  const char* description;
  const char* driverInfo;
  const char* driverVersion;
  int         interfaceVersion;
};

// ---------------------------------------------------------------------------
// Helper per parsing parametri Alpaca e costruzione risposte JSON standard
// ---------------------------------------------------------------------------
class AlpacaHelper {
public:
  static uint32_t nextServerTransactionID();

  static uint32_t getClientTransactionID(WebServer &server);
  static uint32_t getClientID(WebServer &server);

  static void sendBool(WebServer &server, bool value, uint32_t clientTransactionID,
                        int errorNumber = AlpacaError::OK, const String &errorMessage = "");
  static void sendDouble(WebServer &server, double value, uint32_t clientTransactionID,
                          int errorNumber = AlpacaError::OK, const String &errorMessage = "");
  static void sendInt(WebServer &server, int value, uint32_t clientTransactionID,
                       int errorNumber = AlpacaError::OK, const String &errorMessage = "");
  static void sendString(WebServer &server, const String &value, uint32_t clientTransactionID,
                          int errorNumber = AlpacaError::OK, const String &errorMessage = "");
  static void sendStringArray(WebServer &server, const char *const values[], size_t count,
                               uint32_t clientTransactionID);
  static void sendEmptyOk(WebServer &server, uint32_t clientTransactionID);
  static void sendError(WebServer &server, int errorNumber, const String &errorMessage,
                         uint32_t clientTransactionID);

  static int    pathArgToInt(WebServer &server, uint8_t index, int defaultValue = 0);
  static int    queryArgToInt(WebServer &server, const char *name, int defaultValue = 0);
  static double queryArgToDouble(WebServer &server, const char *name, double defaultValue = 0.0);
  static bool   queryArgToBool(WebServer &server, const char *name, bool defaultValue = false);
};

struct AlpacaDeviceRef {
  const AlpacaDeviceInfo *info;   // nullptr = device number non valido
  bool                   *connected;

  AlpacaDeviceRef() : info(nullptr), connected(nullptr) {}
  AlpacaDeviceRef(const AlpacaDeviceInfo *i, bool *c) : info(i), connected(c) {}
};

using AlpacaDeviceResolver = AlpacaDeviceRef (*)(int deviceNumber);

// Registra i metodi comuni ASCOM UNA VOLTA per device type. Il device viene
// risolto a ogni richiesta dal numero nel path.
void registerCommonDeviceEndpoints(WebServer &server, const char *deviceType,
                                    AlpacaDeviceResolver resolver);