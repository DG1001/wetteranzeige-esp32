#include "log.h"

#include <freertos/semphr.h>
#include <time.h>

namespace {

constexpr int LINES = 64;
constexpr int WIDTH = 112;

char lines[LINES][WIDTH];
int next = 0;
int count = 0;
SemaphoreHandle_t lock = nullptr;

SemaphoreHandle_t mutex() {
  if (!lock) lock = xSemaphoreCreateMutex();
  return lock;
}

}  // namespace

void logf(const char* fmt, ...) {
  char msg[WIDTH - 10];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  Serial.println(msg);

  // Zeitstempel: Ortszeit, solange die Uhr noch nicht gestellt ist Laufzeit in Sekunden
  char stamp[12];
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  if (tm.tm_year > 2020 - 1900) {
    strftime(stamp, sizeof(stamp), "%H:%M:%S", &tm);
  } else {
    snprintf(stamp, sizeof(stamp), "+%lus", millis() / 1000);
  }

  xSemaphoreTake(mutex(), portMAX_DELAY);
  snprintf(lines[next], WIDTH, "%s %s", stamp, msg);
  next = (next + 1) % LINES;
  if (count < LINES) count++;
  xSemaphoreGive(mutex());
}

void logLines(void (*fn)(const char* line, void* ctx), void* ctx) {
  xSemaphoreTake(mutex(), portMAX_DELAY);
  for (int i = 0; i < count; i++) fn(lines[(next - count + i + LINES) % LINES], ctx);
  xSemaphoreGive(mutex());
}
