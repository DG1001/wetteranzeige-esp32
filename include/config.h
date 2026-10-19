#pragma once

// Vorgaben für den allerersten Start; danach gelten die Einstellungen von der Webseite (NVS).
// Standort: bei leerem Namen wird der Ort automatisch über die IP-Adresse bestimmt.
// Beispiel für einen festen Ort:  "Frankfurt", 50.1109f, 8.6821f
#define LOCATION_NAME "Künzelsau"
#define LOCATION_LAT 49.2820f
#define LOCATION_LON 9.6890f

// Abstand zwischen zwei Wetterabrufen
#define WEATHER_INTERVAL_MS (10UL * 60UL * 1000UL)
// DWD-Regionen für Pollenflug und Biowetter (Liste in src/health.cpp), Vorgabe für den ersten Start
#define POLLEN_REGION 112  // Hohenlohe/mittlerer Neckar/Oberschwaben
#define BIO_ZONE 'I'       // Württemberg, Franken

// Abstand der Abrufe von Unwetterwarnungen (DWD) und Luftqualität/Pollen
#define WARNINGS_INTERVAL_MS (5UL * 60UL * 1000UL)
#define AIR_INTERVAL_MS (30UL * 60UL * 1000UL)
// Pollenflug und Biowetter erscheinen einmal täglich gegen 11 Uhr, die Gartenbilanz ändert sich langsam
#define HEALTH_INTERVAL_MS (60UL * 60UL * 1000UL)
#define GARDEN_INTERVAL_MS (3UL * 60UL * 60UL * 1000UL)
// Wartezeit bis zum nächsten Versuch, wenn ein Abruf fehlschlägt
#define WEATHER_RETRY_MS (60UL * 1000UL)

// Helligkeit der Hintergrundbeleuchtung (0-255) tagsüber und nachts (Vorgabe für den ersten Start)
#define BRIGHTNESS_DAY 220
#define BRIGHTNESS_NIGHT 60
