#include "ws_debug.h"
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <stdarg.h>

static WebSocketsServer *g_ws = nullptr;
static String g_lineBuf;

void wsDebugSetup(WebSocketsServer *ws) {
  g_ws = ws;
}

static void wsDebugSend(const String &line) {
  if (!g_ws) return;
  JsonDocument doc;
  doc["event"] = "debug";
  doc["msg"] = line;   // ArduinoJson gestisce l'escaping
  String out;
  serializeJson(doc, out);
  g_ws->broadcastTXT(out);
}

void wsDebugPrint(const String &s) {
  g_lineBuf += s;
}

void wsDebugPrintln(const String &s) {
  g_lineBuf += s;
  wsDebugSend(g_lineBuf);
  g_lineBuf = "";
}

void wsDebugf(const char *fmt, ...) {
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  wsDebugSend(buf);
}