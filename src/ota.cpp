#include "ota.h"

#include <Update.h>
#include <StreamString.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

namespace {

const char OTA_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang='it'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'/>
  <title>OTA Update</title>
</head>
<body>
  <h3>Firmware</h3>
  <form method='POST' action='' enctype='multipart/form-data'>
    <input type='file' accept='.bin' name='firmware'>
    <input type='submit' value='Update firmware'>
  </form>
  <h3>Filesystem (LittleFS)</h3>
  <form method='POST' action='' enctype='multipart/form-data'>
    <input type='file' accept='.bin' name='filesystem'>
    <input type='submit' value='Update filesystem'>
  </form>
</body>
</html>)HTML";

OtaDoneCallback g_onDone;
bool g_isFilesystem = false;
bool g_error = false;
String g_errorMsg;
bool g_fsUnmounted = false;

bool g_firstChunk = true;

bool failValidation(const char *msg) {
  g_error = true;
  g_errorMsg = msg;
  Update.abort();
  return false;
}

// Controlla solo i primi byte del file, prima di scrivere in flash.
// Layout immagine ESP32: [0]=0xE9, [12..13]=chip id (0 = ESP32),
// [32..35]=magic dell'app descriptor (0xABCD5432, little-endian).
bool validateFirstChunk(const uint8_t *b, size_t n) {
  const bool looksLikeApp = (n >= 1 && b[0] == 0xE9);

  if (g_isFilesystem) {
    if (looksLikeApp)
      return failValidation("This is a firmware image, not a filesystem image");
    return true;
  }

  if (n < 36 || !looksLikeApp)
    return failValidation("Not a valid ESP32 firmware image (bad magic byte)");
  if ((b[12] | (b[13] << 8)) != 0)
    return failValidation("Firmware built for a different chip (not ESP32)");
  if (b[32] != 0x32 || b[33] != 0x54 || b[34] != 0xCD || b[35] != 0xAB)
    return failValidation("Firmware image has no valid app descriptor");
  return true;
}

void remountFsIfNeeded() {
  if (!g_fsUnmounted) return;
  g_fsUnmounted = false;
  LittleFS.begin(false);   // false: non formattare, tenta solo il mount
}

void reportError(const __FlashStringHelper *context) {
  g_error = true;
  StreamString s;
  Update.printError(s);
  g_errorMsg = String(context) + ": " + s;
}

void handleUploadStart(HTTPUpload &upload) {
  g_error = false;
  g_errorMsg = "";
  g_isFilesystem = (upload.name == "filesystem");
  g_firstChunk = true;

  if (g_isFilesystem) {
    // Rilascia il filesystem prima di sovrascrivere la partizione dati:
    // evita che LittleFS tenga file/handle aperti mentre la partizione
    // sottostante viene riscritta byte a byte.
    LittleFS.end();
    g_fsUnmounted = true;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
      reportError(F("begin filesystem"));
    }
  } else {
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace, U_FLASH)) {
      reportError(F("begin firmware"));
    }
  }
}

void handleUploadWrite(HTTPUpload &upload) {
  esp_task_wdt_reset();
  if (g_error) return;

  if (g_firstChunk) {
    g_firstChunk = false;
    if (! validateFirstChunk(upload.buf, upload.currentSize)) return;
  }

  if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
    reportError(F("write"));
  }
}

void handleUploadEnd() {
  if (g_error) return;
  if (!Update.end(true)) {   // true: imposta la dimensione finale al progresso corrente
    reportError(F("end"));
  }
}

void handleUploadAborted() {
  Update.abort();
  g_error = true;
  g_errorMsg = "Upload interrotto dal client";
  remountFsIfNeeded();
}

} // namespace

void otaSetup(WebServer &server, const char *path, OtaDoneCallback onDone) {
  g_onDone = onDone;

  server.on(path, HTTP_GET, [&server]() {
    server.send_P(200, "text/html", OTA_PAGE);
  });

  server.on(path, HTTP_POST, [&server]() {
    // Chiamato dopo l'ultimo UPLOAD_FILE_END: la scrittura e' gia' conclusa
    // (con successo o errore), qui ci si limita a rispondere al client.
    server.sendHeader("Connection", "close");
    if (g_error) {
      remountFsIfNeeded();
      server.send(500, "text/plain", "Update failed - " + g_errorMsg);
    } else {
     // Auto-refresh lato client verso "/": 10s coprono il riavvio del device
      // e la riconnessione WiFi/mDNS prima che il browser ricarichi la pagina.
      String msg = g_isFilesystem ? "Filesystem " : "Firmware ";
      String html = "<!DOCTYPE html><html lang='it'><head><meta charset='utf-8'>"
                    "<meta http-equiv='refresh' content='15;url=/'>"
                    "<title>OTA Update</title></head><body>"
                    "<p>" + msg + "update, rebooting...</p>"
                    "</body></html>";
      server.send(200, "text/html", html);     
    }
    if (g_onDone) g_onDone(!g_error, g_isFilesystem);
  }, [&server]() {
    HTTPUpload &upload = server.upload();
    switch (upload.status) {
      case UPLOAD_FILE_START:   handleUploadStart(upload); break;
      case UPLOAD_FILE_WRITE:   handleUploadWrite(upload);  break;
      case UPLOAD_FILE_END:     handleUploadEnd();          break;
      case UPLOAD_FILE_ABORTED: handleUploadAborted();      break;
      default: break;
    }
  });
}
