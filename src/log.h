#pragma once

#include <Arduino.h>

// Meldung auf die serielle Schnittstelle und in einen Ringpuffer (für die Statusseite); aus jedem Task nutzbar
void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// Ruft fn für jede gespeicherte Zeile auf, älteste zuerst
void logLines(void (*fn)(const char* line, void* ctx), void* ctx);
