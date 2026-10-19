#include "outdoor.h"

#include <math.h>

#include <algorithm>

namespace {

bool isSnow(int code) { return (code >= 71 && code <= 77) || code == 85 || code == 86; }

int hourScore(const Weather& w, int i) {
  if (!w.hIsDay[i]) return 0;
  float s = 100 - w.hProb[i] * 0.8f;
  if (w.hPrecip[i] >= 0.1f) s -= 50;
  float t = w.hTemp[i];
  if (t < 15) s -= fminf(35, (15 - t) * 2.5f);
  if (t > 26) s -= fminf(50, (t - 26) * 5);
  if (w.hWind[i] > 20) s -= (w.hWind[i] - 20) * 1.5f;
  if (w.hGust[i] > 45) s -= 20;
  int code = w.hCode[i];
  if (code <= 1) s += 5;   // Sonne
  if (code == 3) s -= 8;   // bedeckt
  if (code >= 95) {
    s -= 80;
  } else if (isSnow(code)) {
    s -= 30;
  } else if (code == 45 || code == 48) {
    s -= 15;
  }
  return std::clamp((int)lroundf(s), 0, 100);
}

// „um 16 Uhr“ bzw. „morgen um 16 Uhr“
void when(const Weather& w, int i, char* buf, size_t len) {
  snprintf(buf, len, "%sum %d Uhr", w.hDay[i] > Weather::TODAY ? "morgen " : "", w.hHour[i]);
}

void add(Outdoor& o, HintKind kind, int priority, const char* fmt, ...) {
  if (o.hintCount == (int)(sizeof(o.hints) / sizeof(o.hints[0]))) return;
  Hint& h = o.hints[o.hintCount++];
  h.kind = kind;
  h.priority = priority;
  va_list args;
  va_start(args, fmt);
  vsnprintf(h.text, sizeof(h.text), fmt, args);
  va_end(args);
}

void findWindow(const Weather& w, Outdoor& o) {
  // Heute, solange noch mindestens 2 Stunden Tageslicht bleiben, sonst morgen
  int daylightLeft = 0;
  for (int i = w.nowHour; i < w.hours && w.hDay[i] == Weather::TODAY; i++) daylightLeft += w.hIsDay[i];
  o.day = daylightLeft >= 2 ? Weather::TODAY : min(Weather::TODAY + 1, w.days - 1);

  o.firstIndex = -1;
  o.count = 0;
  for (int i = 0; i < w.hours && o.count < (int)sizeof(o.score); i++) {
    if (w.hDay[i] != o.day) continue;
    if (o.firstIndex < 0) o.firstIndex = i;
    bool past = o.day == Weather::TODAY && i < w.nowHour;
    o.score[o.count++] = past ? 0 : hourScore(w, i);
  }

  // Zusammenhängende Stunden nahe am besten Wert des Tages (mindestens GOOD_SCORE), größte Summe gewinnt.
  // So wird bei durchweg ordentlichem Wetter nicht der ganze Tag, sondern die schönste Zeit genannt.
  int top = 0;
  for (int k = 0; k < o.count; k++) top = max(top, (int)o.score[k]);
  const int threshold = max(GOOD_SCORE, top - 12);
  int bestStart = -1, bestLen = 0, runStart = -1;
  float bestSum = 0, runSum = 0;
  for (int k = 0; k <= o.count; k++) {
    bool good = k < o.count && o.score[k] >= threshold;
    if (good) {
      if (runStart < 0) {
        runStart = k;
        runSum = 0;
      }
      runSum += o.score[k] - threshold + 10;
    } else if (runStart >= 0) {
      if (runSum > bestSum) {
        bestSum = runSum;
        bestStart = runStart;
        bestLen = k - runStart;
      }
      runStart = -1;
    }
  }
  o.found = bestStart >= 0;
  if (!o.found) {
    strlcpy(o.summary, "", sizeof(o.summary));
    return;
  }

  int first = o.firstIndex + bestStart;
  o.startHour = w.hHour[first];
  o.endHour = w.hHour[first + bestLen - 1] + 1;
  float tMin = w.hTemp[first], tMax = tMin, windMax = 0;
  bool dry = true;
  for (int i = first; i < first + bestLen; i++) {
    tMin = fminf(tMin, w.hTemp[i]);
    tMax = fmaxf(tMax, w.hTemp[i]);
    windMax = fmaxf(windMax, w.hWind[i]);
    dry &= w.hProb[i] < 20 && w.hPrecip[i] < 0.1f;
  }
  char temps[16];
  int lo = lroundf(tMin), hi = lroundf(tMax);
  if (lo == hi) {
    snprintf(temps, sizeof(temps), "%d°", lo);
  } else {
    snprintf(temps, sizeof(temps), "%d–%d°", lo, hi);
  }
  snprintf(o.summary, sizeof(o.summary), "%s · %s · %s", dry ? "trocken" : "kaum Regen", temps,
           windMax < 12 ? "wenig Wind" : windMax < 25 ? "leichter Wind" : "windig");
}

void findHints(const Weather& w, const Health& health, const Garden& garden, Outdoor& o) {
  o.hintCount = 0;
  const int end = min(w.hours, w.nowHour + 24);
  char at[32];
  int thunder = -1, ice = -1, snow = -1, fog = -1, umbrella = -1;
  int gustAt = w.nowHour, rainAt = w.nowHour, coldAt = w.nowHour;
  for (int i = w.nowHour; i < end; i++) {
    if (thunder < 0 && w.hCode[i] >= 95) thunder = i;
    if (ice < 0 && w.hTemp[i] <= 1 && w.hPrecip[i] >= 0.1f) ice = i;
    if (snow < 0 && isSnow(w.hCode[i])) snow = i;
    if (fog < 0 && i < w.nowHour + 12 && (w.hCode[i] == 45 || w.hCode[i] == 48)) fog = i;
    if (umbrella < 0 && w.hIsDay[i] && w.hProb[i] >= 60) umbrella = i;
    if (w.hGust[i] > w.hGust[gustAt]) gustAt = i;
    if (w.hPrecip[i] > w.hPrecip[rainAt]) rainAt = i;
    if (!w.hIsDay[i] && w.hTemp[i] < w.hTemp[coldAt]) coldAt = i;
  }

  if (thunder >= 0) {
    when(w, thunder, at, sizeof(at));
    add(o, HintKind::Thunder, 9, "Gewitter möglich %s", at);
  }
  if (ice >= 0) {
    when(w, ice, at, sizeof(at));
    add(o, HintKind::Ice, 9, "Glättegefahr %s", at);
  }
  if (w.hGust[gustAt] >= 65) {
    when(w, gustAt, at, sizeof(at));
    add(o, HintKind::Storm, 8, "Sturmböen bis %d km/h %s", (int)lroundf(w.hGust[gustAt]), at);
  } else if (w.hGust[gustAt] >= 50) {
    when(w, gustAt, at, sizeof(at));
    add(o, HintKind::Wind, 4, "Windig, Böen bis %d km/h %s", (int)lroundf(w.hGust[gustAt]), at);
  }
  if (w.hPrecip[rainAt] >= 5) {
    when(w, rainAt, at, sizeof(at));
    add(o, HintKind::HeavyRain, 8, "Starkregen %s (%d mm)", at, (int)lroundf(w.hPrecip[rainAt]));
  }
  if (snow >= 0) {
    when(w, snow, at, sizeof(at));
    add(o, HintKind::Snow, 7, "Schnee %s", at);
  }
  if (!w.hIsDay[coldAt] && w.hTemp[coldAt] <= 0) {
    add(o, HintKind::Frost, 7, "Frost bis %d°: Pflanzen schützen", (int)lroundf(w.hTemp[coldAt]));
  } else if (!w.hIsDay[coldAt] && w.hTemp[coldAt] <= 3) {
    add(o, HintKind::GroundFrost, 5, "Bodenfrost möglich (%d° in der Nacht)", (int)lroundf(w.hTemp[coldAt]));
  }
  if (umbrella >= 0) {
    when(w, umbrella, at, sizeof(at));
    add(o, HintKind::Umbrella, 6, "Schirm mitnehmen: Regen %s", at);
  }

  const DayForecast& d = w.day[o.day];
  const char* which = o.day == Weather::TODAY ? "heute" : "morgen";
  if (d.tMax >= 30) add(o, HintKind::Heat, 6, "Heiß bis %d° %s: viel trinken", (int)lroundf(d.tMax), which);
  if (d.uvMax >= 6) add(o, HintKind::Uv, 5, "UV-Index %s bis %d: Sonnenschutz", which, (int)lroundf(d.uvMax));
  if (fog >= 0) {
    when(w, fog, at, sizeof(at));
    add(o, HintKind::Fog, 4, "Nebel %s", at);
  }

  // Wäsche: mindestens 5 trockene Stunden mit Sonne oder Wind am Tag des Zeitfensters
  int runStart = -1, bestStart = -1, bestLen = 0;
  for (int k = 0; k <= o.count; k++) {
    int i = o.firstIndex + k;
    bool ok = k < o.count && w.hIsDay[i] && (o.day != Weather::TODAY || i >= w.nowHour) && w.hProb[i] < 20 &&
              w.hPrecip[i] < 0.1f && w.hTemp[i] >= 8 && (w.hHumidity[i] < 75 || w.hWind[i] >= 10);
    if (ok && runStart < 0) runStart = k;
    if (!ok && runStart >= 0) {
      if (k - runStart > bestLen) {
        bestLen = k - runStart;
        bestStart = runStart;
      }
      runStart = -1;
    }
  }
  if (bestLen >= 5) {
    int first = o.firstIndex + bestStart;
    add(o, HintKind::Laundry, 3, "Gutes Wetter zum Wäschetrocknen (%d–%d Uhr)", w.hHour[first],
        w.hHour[first + bestLen - 1] + 1);
  }

  // Pollenflug heute ab „mittel“ (Stufe 2): stärkste Pollenart nennen
  if (health.pollenValid) {
    int top = -1;
    for (int p = 0; p < Health::POLLEN; p++) {
      if (top < 0 || health.pollen[p][0] > health.pollen[top][0]) top = p;
    }
    int level = health.pollen[top][0];
    if (level >= 4) {
      add(o, HintKind::Pollen, level >= 6 ? 6 : 5, "Pollenflug heute: %s %s", POLLEN_NAMES[top],
          level >= 6 ? "hoch" : level == 5 ? "mittel bis hoch" : "mittel");
    }
  }
  // Biowetter: nur hohe Belastungen
  if (health.bioValid) {
    for (int e = 0; e < Health::EFFECTS; e++) {
      if (health.bio[e][0] >= 4) {
        add(o, HintKind::Bio, 4, "Biowetter: hohe Belastung (%s)", BIO_NAMES[e]);
        break;
      }
    }
  }
  // Gießen: deutlich zu wenig Regen und keiner in Sicht
  if (garden.valid && garden.balance <= -10 && garden.rainSoon < 5) {
    add(o, HintKind::Water, 4, "Gießen: in 7 Tagen %d mm zu wenig Regen", (int)lroundf(-garden.balance));
  }

  std::stable_sort(o.hints, o.hints + o.hintCount, [](const Hint& a, const Hint& b) { return a.priority > b.priority; });
}

}  // namespace

void evaluateOutdoor(const Weather& w, const Health& health, const Garden& garden, Outdoor& out) {
  if (!w.valid) return;
  findWindow(w, out);
  findHints(w, health, garden, out);
}

void bestTimeText(const Outdoor& o, char* buf, size_t len) {
  bool today = o.day == Weather::TODAY;
  if (o.found) {
    snprintf(buf, len, "%s ideal: %d–%d Uhr", today ? "Draußen" : "Morgen", o.startHour, o.endHour);
  } else {
    snprintf(buf, len, "%s kein Draußen-Wetter", today ? "Heute" : "Morgen");
  }
}
