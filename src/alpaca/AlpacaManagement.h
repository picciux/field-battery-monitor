#pragma once

#include <WebServer.h>

// Registra /management/apiversions, /management/v1/description,
// /management/v1/configureddevices. Necessario perche' i client Alpaca
// (N.I.N.A., ASCOM Remote, ecc.) trovino i tuoi device via discovery.
void alpacaManagementSetup(WebServer &server);
