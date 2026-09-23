#pragma once
#include <Arduino.h>

class WebSocketsServer;

// Da chiamare una sola volta dopo webSocket.begin(). Se non chiamata (o se
// il backend attivo non e' quello websocket), le funzioni sotto sono no-op
// sicuri: g_ws resta nullptr e wsDebugSend scarta silenziosamente.
void wsDebugSetup(WebSocketsServer *ws);

// Stile Print (come Serial.print/println): piu' chiamate a wsDebugPrint()
// accumulano sulla stessa riga, wsDebugPrintln() la chiude e la invia come
// un unico evento {"event":"debug","msg":"..."}.
void wsDebugPrint(const String &s);
void wsDebugPrintln(const String &s);

// Stile printf: costruisce e invia una riga completa in un colpo.
// Buffer fisso, troncato a 160 caratteri.
void wsDebugf(const char *fmt, ...);