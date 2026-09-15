#pragma once

#include <WebServer.h>

// Registra gli endpoint REST IObservingConditionsV1 su device_number = 0.
// Al momento e' uno scheletro "vuoto": tutte le proprieta' meteo restituiscono
// NotImplementedException finche' non colleghi sensori reali (pioggia, vento,
// umidita', pressione, ecc.). La batteria NON e' qui: e' gestita come Switch
// (vedi AlpacaSwitch.h).
void alpacaObservingConditionsSetup(WebServer &server);
