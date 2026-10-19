#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "air.h"
#include "garden.h"
#include "health.h"
#include "outdoor.h"
#include "warnings.h"
#include "weather.h"

// Seiten; die ersten sechs erreicht man per Wischen, die übrigen per Antippen, langes Drücken bzw. nachts
enum class Page { Overview, Outdoor, Radar, Hours, SkyAir, Health, Day, Warnings, Night, Info };
constexpr int MAIN_PAGES = 6;

struct Model {
  Location loc;
  Weather weather;
  Air air;
  Warnings warnings;
  Outdoor outdoor;
  Health health;
  Garden garden;

  Page page = Page::Overview;
  int day = Weather::TODAY;  // Tag für die Tagesansicht
  bool radarPaused = false;

  char status[64] = "";  // klein unter der Uhr
  bool statusWarn = false;
};

// Schützt Model: Der Hintergrund-Task schreibt neue Daten, die Oberfläche liest beim Zeichnen
SemaphoreHandle_t modelMutex();

struct ModelLock {
  ModelLock() { xSemaphoreTakeRecursive(modelMutex(), portMAX_DELAY); }
  ~ModelLock() { xSemaphoreGiveRecursive(modelMutex()); }
  ModelLock(const ModelLock&) = delete;
  ModelLock& operator=(const ModelLock&) = delete;
};
