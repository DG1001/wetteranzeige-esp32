#include "net.h"

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
    if (https_) secure_.setInsecure();
    ok_ = http_.begin(https_ ? static_cast<WiFiClient&>(secure_) : plain_, url);
    // HTTP/1.0 verhindert Chunked-Encoding, damit der Datenstrom direkt lesbar ist
    http_.useHTTP10(true);
    http_.setTimeout(10000);
    http_.addHeader("User-Agent", "esp32-wetteranzeige");
    if (ok_) {
      int status = http_.GET();
      if (status != HTTP_CODE_OK) {
        Serial.printf("HTTP %d bei %s\n", status, url.c_str());
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

bool httpGetJson(const String& url, JsonDocument& doc) {
  Request req(url);
  if (!req.ok()) return false;
  DeserializationError err = deserializeJson(doc, req.http().getStream());
  if (err) {
    Serial.printf("JSON-Fehler: %s bei %s\n", err.c_str(), url.c_str());
    return false;
  }
  return true;
}

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
    free(buf);
    return nullptr;
  }
  return buf;
}
