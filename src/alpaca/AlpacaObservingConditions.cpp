#include "AlpacaObservingConditions.h"
#include "AlpacaCommon.h"
#include <uri/UriBraces.h>

static bool g_ocConnected = true;
static double g_averagePeriod = 0.0; // ore, 0 = istantaneo (valore legale minimo per spec)

static AlpacaDeviceInfo g_ocInfo = {
  "ESP32 Observing Conditions",
  "ESP32 Alpaca ObservingConditions device - scheletro generico",
  "ESP32 Alpaca ObservingConditions Driver",
  "1.0",
  1 // IObservingConditionsV1
};

// Nomi delle proprieta' meteo opzionali: quando non hai il sensore collegato,
// la spec Alpaca prevede che tu risponda NotImplementedException (non un errore
// HTTP, ma nel corpo JSON) invece di un valore finto.
static const char *const NOT_IMPLEMENTED_PROPS[] = {
  "cloudcover", "dewpoint", "humidity", "pressure", "rainrate",
  "skybrightness", "skyquality", "skytemperature", "starfwhm",
  "temperature", "winddirection", "windgust", "windspeed"
};
static const int NUM_NOT_IMPLEMENTED_PROPS =
    sizeof(NOT_IMPLEMENTED_PROPS) / sizeof(NOT_IMPLEMENTED_PROPS[0]);

static AlpacaDeviceRef ocResolver(int number) {
  if (number != 0) return {};
  return AlpacaDeviceRef(&g_ocInfo, &g_ocConnected);
}

void alpacaObservingConditionsSetup(WebServer &server) {
  registerCommonDeviceEndpoints(server, "observingconditions", ocResolver);

  const String base = "/api/v1/observingconditions/{}/";

  // averageperiod: unica proprieta' realmente "attiva" di default (richiesta da spec)
  server.on(UriBraces(base + "averageperiod"), HTTP_GET, [&server]() {
    AlpacaHelper::sendDouble(server, g_averagePeriod, AlpacaHelper::getClientTransactionID(server));
  });

  server.on(UriBraces(base + "averageperiod"), HTTP_PUT, [&server]() {
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    double v = AlpacaHelper::queryArgToDouble(server, "AveragePeriod", -1.0);
    if (v < 0.0) {
      AlpacaHelper::sendError(server, AlpacaError::InvalidValue, "AveragePeriod non valido", ctid);
      return;
    }
    g_averagePeriod = v;
    AlpacaHelper::sendEmptyOk(server, ctid);
  });

  // refresh: forza un aggiornamento sensori (no-op finche' non hai sensori veri)
  server.on(UriBraces(base + "refresh"), HTTP_PUT, [&server]() {
    // TODO: quando aggiungerai sensori reali, richiama qui le loro letture
    AlpacaHelper::sendEmptyOk(server, AlpacaHelper::getClientTransactionID(server));
  });

  // sensordescription(SensorName) / timesincelastupdate(SensorName)
  server.on(UriBraces(base + "sensordescription"), HTTP_GET, [&server]() {
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    AlpacaHelper::sendError(server, AlpacaError::NotImplemented, "Nessun sensore collegato", ctid);
  });

  server.on(UriBraces(base + "timesincelastupdate"), HTTP_GET, [&server]() {
    uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
    AlpacaHelper::sendError(server, AlpacaError::NotImplemented, "Nessun sensore collegato", ctid);
  });

  // Tutte le altre proprieta' meteo: NotImplementedException finche' non
  // colleghi un sensore vero. Per attivarne una, sostituisci la entry
  // corrispondente con un handler dedicato che legge il sensore (vedi
  // esempio commentato sotto per 'temperature').
  for (int i = 0; i < NUM_NOT_IMPLEMENTED_PROPS; i++) {
    String propName = NOT_IMPLEMENTED_PROPS[i];
    server.on(UriBraces(base + propName), HTTP_GET, [&server, propName]() {
      uint32_t ctid = AlpacaHelper::getClientTransactionID(server);
      AlpacaHelper::sendError(server, AlpacaError::NotImplemented,
                               propName + " non implementata", ctid);
    });
  }

  // Esempio di come "promuovere" una proprieta' da stub a reale in futuro:
  //
  // server.on(UriBraces(base + "temperature"), HTTP_GET, [&server]() {
  //   AlpacaHelper::sendDouble(server, readOutdoorTemperature(),
  //                             AlpacaHelper::getClientTransactionID(server));
  // });
  //
  // (rimuovi "temperature" da NOT_IMPLEMENTED_PROPS quando lo fai, altrimenti
  // l'ultima .on() registrata per lo stesso path vince)
}
