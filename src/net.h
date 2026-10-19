#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Lädt JSON per HTTP(S); mit filter werden nur die darin markierten Felder übernommen
bool httpGetJson(const String& url, JsonDocument& doc);
bool httpGetJson(const String& url, JsonDocument& doc, const JsonDocument& filter);
// Lädt die Antwort in einen Puffer im PSRAM; Freigabe mit free(). nullptr bei Fehler.
uint8_t* httpGetBytes(const String& url, size_t& len, size_t maxLen);
// Prozentkodierung für URL-Parameter (UTF-8 bleibt erhalten)
String urlEncode(const char* s);
