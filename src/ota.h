#pragma once

#include <WebServer.h>
#include <functional>

// Aggiornamento OTA (firmware + filesystem LittleFS) via browser, implementato
// con l'API Update di basso livello invece di HTTPUpdateServer: la libreria
// core ha un bug noto sul ramo filesystem (Update.begin(SPIFFS.totalBytes(),
// U_SPIFFS) puo' fallire con "Bad Size Given" - vedi
// https://github.com/espressif/arduino-esp32/issues/9967 - ancora piu' rilevante
// qui perche' il progetto usa LittleFS, non SPIFFS). Con UPDATE_SIZE_UNKNOWN in
// entrambi i rami (firmware e filesystem) il comportamento e' indipendente
// dalla versione del core installata.
//
// onDone(success, isFilesystem) viene invocato subito dopo la scrittura (prima
// di inviare la risposta HTTP): usarlo per pianificare un restart (es.
// wifiComm.requestRestart(), che gia' rimanda il reboot di 500ms per lasciare
// il tempo alla risposta di uscire sul socket). Non chiamare ESP.restart()
// direttamente da qui.
using OtaDoneCallback = std::function<void(bool success, bool isFilesystem)>;

void otaSetup(WebServer &server, const char *path, OtaDoneCallback onDone = nullptr);
