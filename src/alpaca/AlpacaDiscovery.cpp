#include "AlpacaDiscovery.h"
#include <WiFiUdp.h>
#include <string.h>

#define ALPACA_DISCOVERY_PORT   32227
#define ALPACA_DISCOVERY_MAGIC  "alpacadiscovery1"
#define ALPACA_DISCOVERY_BUF    64

static WiFiUDP g_udp;
static uint16_t g_alpacaPort = 0;

void alpacaDiscoverySetup(uint16_t alpacaPort) {
  g_alpacaPort = alpacaPort;
  g_udp.begin(ALPACA_DISCOVERY_PORT);
}

void alpacaDiscoveryRun() {
  int packetSize = g_udp.parsePacket();
  if (packetSize <= 0) return;

  char buf[ALPACA_DISCOVERY_BUF];
  int len = g_udp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  // Il client manda un pacchetto di testo ASCII, esattamente questa stringa
  // (case-sensitive, da spec Alpaca). Un pacchetto non riconosciuto (rumore
  // di rete, altri protocolli sulla stessa porta) non genera risposta.
  if (strcmp(buf, ALPACA_DISCOVERY_MAGIC) != 0) return;

  char response[32];
  int rlen = snprintf(response, sizeof(response), "{\"AlpacaPort\":%u}", g_alpacaPort);

  g_udp.beginPacket(g_udp.remoteIP(), g_udp.remotePort());
  g_udp.write((const uint8_t *) response, rlen);
  g_udp.endPacket();
}