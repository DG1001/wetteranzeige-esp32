#pragma once

// Standort: bei leerem Namen wird der Ort automatisch über die IP-Adresse bestimmt.
// Beispiel für einen festen Ort:  "Frankfurt", 50.1109f, 8.6821f
#define LOCATION_NAME "Künzelsau"
#define LOCATION_LAT 49.2820f
#define LOCATION_LON 9.6890f

// Abstand zwischen zwei Wetterabrufen
#define WEATHER_INTERVAL_MS (10UL * 60UL * 1000UL)
// Abstand der Abrufe von Unwetterwarnungen (DWD) und Luftqualität/Pollen
#define WARNINGS_INTERVAL_MS (5UL * 60UL * 1000UL)
#define AIR_INTERVAL_MS (30UL * 60UL * 1000UL)
// Wartezeit bis zum nächsten Versuch, wenn ein Abruf fehlschlägt
#define WEATHER_RETRY_MS (60UL * 1000UL)

// Helligkeit der Hintergrundbeleuchtung (0-255) tagsüber und nachts
#define BRIGHTNESS_DAY 220
#define BRIGHTNESS_NIGHT 60
