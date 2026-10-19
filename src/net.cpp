#include "net.h"

#include "log.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

namespace {

// Hält Client und Verbindung für die Dauer eines Abrufs
class Request {
 public:
  explicit Request(const String& url) {
    https_ = url.startsWith("https");
    if (https_) {
      // Zertifikate gegen die Sammlung von ESP-IDF prüfen
      secure_.useBuiltinCACertBundle();
      secure_.setHandshakeTimeout(15);
    }
    ok_ = http_.begin(https_ ? static_cast<WiFiClient&>(secure_) : plain_, url);
    // HTTP/1.0 verhindert Chunked-Encoding, damit der Datenstrom direkt lesbar ist
    http_.useHTTP10(true);
    http_.setTimeout(10000);
    http_.addHeader("User-Agent", "esp32-wetteranzeige");
    if (ok_) {
      int status = http_.GET();
      if (status != HTTP_CODE_OK) {
        logf("HTTP %d bei %.60s", status, url.c_str());
        ok_ = false;
      }
    }
  }
  ~Request() { http_.end(); }

  bool ok() const { return ok_; }
  HTTPClient& http() { return http_; }

 private:
  WiFiClientSecure secure_;
  WiFiClient plain_;
  HTTPClient http_;
  bool https_ = false;
  bool ok_ = false;
};

}  // namespace

namespace {

// Erst vollständig in den PSRAM laden, dann auswerten: Direkt aus dem Datenstrom brach das Einlesen
// großer Dateien (Biowetter, 109 KB) bei Übertragungspausen ab
bool getJson(const String& url, JsonDocument& doc, const JsonDocument* filter) {
  size_t len;
  uint8_t* body = httpGetBytes(url, len, 512 * 1024);
  if (!body) return false;
  const char* text = reinterpret_cast<const char*>(body);
  DeserializationError err = filter ? deserializeJson(doc, text, len, DeserializationOption::Filter(*filter))
                                    : deserializeJson(doc, text, len);
  free(body);
  if (err) {
    logf("JSON-Fehler: %s bei %.60s", err.c_str(), url.c_str());
    return false;
  }
  return true;
}

}  // namespace

bool httpGetJson(const String& url, JsonDocument& doc) { return getJson(url, doc, nullptr); }

bool httpGetJson(const String& url, JsonDocument& doc, const JsonDocument& filter) { return getJson(url, doc, &filter); }

uint8_t* httpGetBytes(const String& url, size_t& len, size_t maxLen) {
  Request req(url);
  if (!req.ok()) return nullptr;

  int size = req.http().getSize();
  size_t capacity = size > 0 ? size : 32 * 1024;
  if (capacity > maxLen) return nullptr;
  auto* buf = (uint8_t*)heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM);
  if (!buf) return nullptr;

  WiFiClient* stream = req.http().getStreamPtr();
  len = 0;
  uint32_t deadline = millis() + 15000;
  while ((size < 0 || len < (size_t)size) && millis() < deadline) {
    if (!stream->connected() && !stream->available()) break;
    int avail = stream->available();
    if (avail <= 0) {
      delay(2);
      continue;
    }
    if (len + avail > capacity) {
      size_t grown = min(maxLen, capacity * 2);
      if (grown <= capacity) break;
      auto* bigger = (uint8_t*)heap_caps_realloc(buf, grown, MALLOC_CAP_SPIRAM);
      if (!bigger) break;
      buf = bigger;
      capacity = grown;
    }
    len += stream->readBytes(buf + len, min((size_t)avail, capacity - len));
  }
  if (len == 0 || (size > 0 && len != (size_t)size)) {
    logf("Unvollständig: %u von %d Bytes bei %.60s", len, size, url.c_str());
    free(buf);
    return nullptr;
  }
  return buf;
}

String urlEncode(const char* s) {
  String out;
  for (const uint8_t* p = (const uint8_t*)s; *p; p++) {
    if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
      out += (char)*p;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", *p);
      out += hex;
    }
  }
  return out;
}
