#include "AlpacaSafetyMonitor.h"
#include "AlpacaCommon.h"
#include <uri/UriBraces.h>

static bool g_smConnected = true;

static AlpacaDeviceInfo g_smInfo = {
  "Battery Safety Monitor",
  "Reports operation safety based on battery state-of-charge.",
  "ESP32 Alpaca SafetyMonitor Driver",
  "1.0",
  1 // ISafetyMonitor
};

const AlpacaDeviceInfo &getSafetyMonitorInfo() { return g_smInfo; }

static AlpacaDeviceRef smResolver(int number) {
  if (number != 0) return {};
  return AlpacaDeviceRef(&g_smInfo, &g_smConnected);
}

void alpacaSafetyMonitorSetup(WebServer &server, Hardware *hardware) {
  registerCommonDeviceEndpoints(server, "safetymonitor", smResolver);

  const String base = "/api/v1/safetymonitor/{}/";

  // issafe: unica proprieta' specifica di questo device type
  server.on(UriBraces(base + "issafe"), HTTP_GET, [&server, hardware]() {
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    if (AlpacaHelper::pathArgToInt(server, 0, -1) != 0) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "Device number out of range", ctid);
      return;
    }
    
    AlpacaHelper::sendBool(server, hardware->battery->isSafe(), ctid);
  });
}


