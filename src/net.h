#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Lädt JSON per HTTP(S)
bool httpGetJson(const String& url, JsonDocument& doc);
// Lädt die Antwort in einen Puffer im PSRAM; Freigabe mit free(). nullptr bei Fehler.
uint8_t* httpGetBytes(const String& url, size_t& len, size_t maxLen);
