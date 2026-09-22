#pragma once
#include <stdint.h>

// Avvia il servizio di auto-discovery Alpaca (porta UDP 32227, broadcast
// "alpacadiscovery1"). Senza questo, i client Alpaca (N.I.N.A., ASCOM
// Remote, ecc.) non trovano l'IP del device in rete: va inserito a mano.
void alpacaDiscoverySetup(uint16_t alpacaPort);

// Da chiamare ad ogni giro di loop: non bloccante.
void alpacaDiscoveryRun();