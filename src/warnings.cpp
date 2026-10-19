#include "warnings.h"

#include <time.h>

#include <algorithm>

#include "astro.h"
#include "log.h"
#include "net.h"

namespace {

int levelOf(const char* severity) {
  if (!strcmp(severity, "Extreme")) return 4;
  if (!strcmp(severity, "Severe")) return 3;
  if (!strcmp(severity, "Moderate")) return 2;
  return 1;
}

// "JJJJ-MM-TTTHH:MM:SSZ" (UTC) in Ortszeit, formatiert als „Mo 14:00“
void localTime(const char* iso, int utcOffset, char* buf, size_t len) {
  static const char* const DAYS[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  if (!iso || strlen(iso) < 16) {
    strlcpy(buf, "?", len);
    return;
  }
  long days = daysFromCivil(atoi(iso), atoi(iso + 5), atoi(iso + 8));
  time_t t = days * 86400L + atoi(iso + 11) * 3600L + atoi(iso + 14) * 60L + utcOffset;
  struct tm tm;
  gmtime_r(&t, &tm);
  snprintf(buf, len, "%s %02d:%02d", DAYS[tm.tm_wday], tm.tm_hour, tm.tm_min);
}

}  // namespace

bool fetchWarnings(const Location& loc, int utcOffset, Warnings& out) {
  char url[512];
  snprintf(url, sizeof(url),
           "https://maps.dwd.de/geoserver/dwd/ows?service=WFS&version=1.0.0&request=GetFeature"
           "&typeName=dwd:Warnungen_Gemeinden&outputFormat=application/json"
           "&propertyName=EVENT,HEADLINE,DESCRIPTION,INSTRUCTION,SEVERITY,ONSET,EXPIRES"
           "&CQL_FILTER=INTERSECTS(THE_GEOM,POINT(%.4f%%20%.4f))",
           loc.lon, loc.lat);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  out.count = 0;
  for (JsonObject f : doc["features"].as<JsonArray>()) {
    if (out.count == Warnings::MAX) break;
    JsonObject p = f["properties"];
    Warning& w = out.item[out.count++];
    w.level = levelOf(p["SEVERITY"] | "");
    strlcpy(w.event, p["EVENT"] | "", sizeof(w.event));
    strlcpy(w.headline, p["HEADLINE"] | "", sizeof(w.headline));
    strlcpy(w.description, p["DESCRIPTION"] | "", sizeof(w.description));
    strlcpy(w.instruction, p["INSTRUCTION"] | "", sizeof(w.instruction));
    char from[16], to[16];
    localTime(p["ONSET"] | "", utcOffset, from, sizeof(from));
    const char* expires = p["EXPIRES"] | "";
    if (*expires) {
      localTime(expires, utcOffset, to, sizeof(to));
      snprintf(w.period, sizeof(w.period), "%s – %s", from, to);
    } else {
      snprintf(w.period, sizeof(w.period), "ab %s", from);
    }
  }
  std::sort(out.item, out.item + out.count, [](const Warning& a, const Warning& b) { return a.level > b.level; });
  out.valid = true;
  logf("Warnungen: %d", out.count);
  return true;
}
