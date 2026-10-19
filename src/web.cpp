#include "web.h"

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

#include "fetch.h"
#include "lcd.h"
#include "log.h"
#include "settings.h"
#include "ui.h"

namespace web {
namespace {

WebServer server(80);
DNSServer dns;
bool portal = false;
const Model* model = nullptr;
void (*changed)(const Changes&) = nullptr;
String networks;  // Gefundene WLANs als <option>-Liste für das Formular

const char STYLE[] PROGMEM = R"(
<style>
body{font-family:-apple-system,system-ui,sans-serif;background:#0e1830;color:#eef2f8;margin:0;padding:16px;
max-width:640px;margin:auto;line-height:1.4}
h1{font-size:1.5em}h2{font-size:1.1em;margin-top:1.6em;color:#a9c1db}
label{display:block;margin:.8em 0 .2em}input,select{width:100%;box-sizing:border-box;padding:.6em;border-radius:8px;
border:1px solid #3a4a66;background:#16233d;color:#eef2f8;font-size:1em}
input[type=checkbox],input[type=radio]{width:auto}.row{display:flex;gap:12px}.row>*{flex:1}
button{margin-top:1.2em;padding:.7em 1.4em;border:0;border-radius:8px;background:#ffc23d;color:#1a1a1a;
font-size:1em;font-weight:600}a{color:#5aaeff}.box{background:#16233d;border-radius:12px;padding:12px 16px}
small{color:#8593ab}img{max-width:100%;border-radius:8px}
</style>)";

String esc(const char* s) {
  String out;
  for (; *s; s++) {
    switch (*s) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += *s;
    }
  }
  return out;
}

String page(const char* title, const String& body) {
  String html = "<!doctype html><html lang=de><head><meta charset=utf-8>"
                "<meta name=viewport content='width=device-width,initial-scale=1'><title>";
  html += title;
  html += "</title>";
  html += FPSTR(STYLE);
  html += "</head><body><h1>";
  html += title;
  html += "</h1>";
  html += body;
  html += "</body></html>";
  return html;
}

String timeText(uint16_t minutes) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", minutes / 60, minutes % 60);
  return buf;
}

uint16_t parseTime(const String& s, uint16_t fallback) {
  int h, m;
  if (sscanf(s.c_str(), "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return fallback;
  return h * 60 + m;
}

uint8_t parseByte(const String& s, uint8_t fallback) {
  if (s.isEmpty()) return fallback;
  return constrain(s.toInt(), 0, 255);
}

bool authorized() {
  if (server.authenticate("admin", settings.pin)) return true;
  server.requestAuthentication(BASIC_AUTH, "Wetteranzeige", "PIN siehe Display (lange drücken)");
  return false;
}

void scanNetworks() {
  int n = WiFi.scanNetworks();
  networks = "";
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty() || networks.indexOf(">" + esc(ssid.c_str()) + "<") >= 0) continue;
    networks += "<option value=\"" + esc(ssid.c_str()) + "\">" + esc(ssid.c_str()) + "</option>";
  }
  WiFi.scanDelete();
}

String wifiFields() {
  String b = "<label>WLAN (2,4 GHz)</label><input name=ssid list=netze value=\"" + esc(settings.ssid) +
             "\" autocomplete=off><datalist id=netze>" + networks + "</datalist>";
  b += "<label>WLAN-Passwort</label><input name=password type=password placeholder='"
       + String(portal ? "" : "leer lassen = unverändert") + "'>";
  return b;
}

// ---------------------------------------------------------------- Normalbetrieb

void handleRoot() {
  if (!authorized()) return;
  String b = "<div class=box>";
  {
    ModelLock lock;
    if (model && model->weather.valid) {
      char line[160];
      snprintf(line, sizeof(line), "<b>%s</b>: %.1f °C · %s<br><small>%s · IP %s</small>", esc(model->loc.city).c_str(),
               model->weather.temp, esc(model->status).c_str(), WiFi.SSID().c_str(),
               WiFi.localIP().toString().c_str());
      b += line;
    } else {
      b += "Noch keine Wetterdaten";
    }
  }
  b += "</div><p><a href=/foto>Bildschirmfoto</a> · <a href=/status>Status und Log</a> · "
       "<a href=/update>Firmware-Update</a></p><p><small>Firmware " FIRMWARE_VERSION
       " · <a href=https://github.com/DG1001/wetteranzeige-esp32/releases>Neue Versionen</a></small></p>";

  b += "<form method=post action=/einstellungen>";
  b += "<h2>Ort</h2><label>Ortsname</label><input name=city value=\"" + esc(settings.city) +
       "\" placeholder='leer = über die IP-Adresse bestimmen'>";
  b += "<small>Koordinaten leer lassen, dann werden sie über den Namen gesucht.</small>";
  b += "<div class=row><div><label>Breite</label><input name=lat value='" +
       (settings.lat || settings.lon ? String(settings.lat, 4) : String()) + "'></div><div><label>Länge</label>"
       "<input name=lon value='" + (settings.lat || settings.lon ? String(settings.lon, 4) : String()) + "'></div></div>";

  b += "<h2>Gesundheit (DWD)</h2><label>Region für den Pollenflug</label><select name=pollenRegion>";
  for (int i = 0; i < POLLEN_REGION_COUNT; i++) {
    b += "<option value=" + String(POLLEN_REGIONS[i].id) + (POLLEN_REGIONS[i].id == settings.pollenRegion ? " selected>" : ">") +
         esc(POLLEN_REGIONS[i].name) + "</option>";
  }
  b += "</select><label>Zone für das Biowetter</label><select name=bioZone>";
  for (int i = 0; i < BIO_ZONE_COUNT; i++) {
    b += String("<option value=") + BIO_ZONES[i].id + (BIO_ZONES[i].id == settings.bioZone ? " selected>" : ">") +
         esc(BIO_ZONES[i].name) + "</option>";
  }
  b += "</select>";

  b += "<h2>Helligkeit</h2><div class=row><div><label>Tag (0–255)</label><input name=brightDay type=number min=0 "
       "max=255 value=" + String(settings.brightDay) + "></div><div><label>Nacht (0–255)</label><input "
       "name=brightNight type=number min=0 max=255 value=" + String(settings.brightNight) + "></div></div>";

  b += "<h2>Nachtmodus</h2><label><input type=checkbox name=nightEnabled value=1";
  if (settings.nightEnabled) b += " checked";
  b += "> eingeschaltet</label><div class=row><div><label>Beginn</label><input name=nightStart type=time value=" +
       timeText(settings.nightStart) + "></div><div><label>Ende</label><input name=nightEnd type=time value=" +
       timeText(settings.nightEnd) + "></div></div>";
  b += "<label><input type=radio name=nightOff value=0";
  if (!settings.nightOff) b += " checked";
  b += "> große Uhr, gedimmt</label><label><input type=radio name=nightOff value=1";
  if (settings.nightOff) b += " checked";
  b += "> Display aus</label><label>Helligkeit der Uhr (0–255)</label><input name=brightClock type=number min=1 "
       "max=255 value=" + String(settings.brightClock) + ">";
  b += "<small>Antippen weckt das Display für eine Minute.</small>";

  b += "<h2>WLAN</h2>" + wifiFields() + "<small>Nach einer Änderung startet die Anzeige neu.</small>";
  b += "<h2>PIN</h2><label>Neue PIN (8 Ziffern, leer = unverändert)</label>"
       "<input name=pin inputmode=numeric pattern='[0-9]{8}'><small>Die PIN ist auch das Passwort des "
       "Einrichtungs-WLANs.</small>";
  b += "<button>Speichern</button></form>";
  server.send(200, "text/html; charset=utf-8", page("Wetteranzeige", b));
}

void handleSave() {
  if (!authorized()) return;
  Changes c;
  String msg;

  String city = server.arg("city"), lat = server.arg("lat"), lon = server.arg("lon");
  city.trim();
  bool cityChanged = city != settings.city;
  float newLat = lat.toFloat(), newLon = lon.toFloat();
  if (cityChanged || newLat != settings.lat || newLon != settings.lon) {
    strlcpy(settings.city, city.c_str(), sizeof(settings.city));
    if (city.length() && (lat.isEmpty() || lon.isEmpty() || cityChanged)) {
      // Koordinaten über den Namen suchen, wenn sie fehlen oder nur der Name geändert wurde
      Location found;
      if (geocode(city.c_str(), found)) {
        strlcpy(settings.city, found.city, sizeof(settings.city));
        settings.lat = found.lat;
        settings.lon = found.lon;
        msg += "Ort gefunden: " + esc(found.city) + " (" + String(found.lat, 4) + ", " + String(found.lon, 4) + ")<br>";
      } else {
        msg += "Ort „" + esc(city.c_str()) + "“ nicht gefunden, bitte Koordinaten angeben.<br>";
        settings.lat = settings.lon = 0;
      }
    } else {
      settings.lat = newLat;
      settings.lon = newLon;
    }
    c.location = true;
  }

  int region = server.arg("pollenRegion").toInt();
  char zone = server.arg("bioZone").length() ? server.arg("bioZone")[0] : settings.bioZone;
  if (region > 0 && region != settings.pollenRegion) {
    settings.pollenRegion = region;
    c.location = true;
  }
  if (zone >= 'A' && zone <= 'K' && zone != settings.bioZone) {
    settings.bioZone = zone;
    c.location = true;
  }

  settings.brightDay = parseByte(server.arg("brightDay"), settings.brightDay);
  settings.brightNight = parseByte(server.arg("brightNight"), settings.brightNight);
  settings.nightEnabled = server.hasArg("nightEnabled");
  settings.nightStart = parseTime(server.arg("nightStart"), settings.nightStart);
  settings.nightEnd = parseTime(server.arg("nightEnd"), settings.nightEnd);
  settings.nightOff = server.arg("nightOff") == "1";
  settings.brightClock = max<uint8_t>(1, parseByte(server.arg("brightClock"), settings.brightClock));
  c.display = true;

  String pin = server.arg("pin");
  // Mindestens 8 Zeichen, weil die PIN auch als WPA2-Passwort des Einrichtungs-WLANs dient
  if (pin.length() == 8) {
    strlcpy(settings.pin, pin.c_str(), sizeof(settings.pin));
    msg += "PIN geändert.<br>";
  }

  String ssid = server.arg("ssid"), password = server.arg("password");
  ssid.trim();
  bool wifiChanged = ssid.length() && (ssid != settings.ssid || password.length());
  if (wifiChanged) {
    strlcpy(settings.ssid, ssid.c_str(), sizeof(settings.ssid));
    if (password.length()) strlcpy(settings.password, password.c_str(), sizeof(settings.password));
  }
  saveSettings();

  if (wifiChanged) {
    server.send(200, "text/html; charset=utf-8",
                page("Gespeichert", msg + "WLAN geändert, die Anzeige startet neu und verbindet sich mit „" +
                                        esc(settings.ssid) + "“."));
    delay(500);
    ESP.restart();
  }
  server.send(200, "text/html; charset=utf-8", page("Gespeichert", msg + "Einstellungen gespeichert. <a href=/>Zurück</a>"));
  if (changed) changed(c);
}

void handlePhoto() {
  if (!authorized()) return;
  server.send(200, "text/html; charset=utf-8",
              page("Bildschirmfoto", "<p><img src=/foto.bmp></p><p><a href=/foto>Neu laden</a> · <a href=/>Zurück</a></p>"));
}

// BMP mit 16 Bit (RGB565, BI_BITFIELDS), von oben nach unten
void handleBitmap() {
  if (!authorized()) return;
  constexpr int W = lcd::WIDTH, H = lcd::HEIGHT, HEADER = 14 + 40 + 12;
  constexpr uint32_t SIZE = HEADER + W * H * 2;
  uint8_t header[HEADER] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; i++) header[at + i] = v >> (8 * i);
  };
  put32(2, SIZE);
  put32(10, HEADER);
  put32(14, 40);
  put32(18, W);
  put32(22, (uint32_t)-H);
  header[26] = 1;
  header[28] = 16;
  put32(30, 3);  // BI_BITFIELDS
  put32(34, W * H * 2);
  put32(54, 0xF800);
  put32(58, 0x07E0);
  put32(62, 0x001F);

  server.setContentLength(SIZE);
  server.send(200, "image/bmp", "");
  server.sendContent((const char*)header, HEADER);
  static uint16_t row[W];
  const uint16_t* fb = lcd::front();
  for (int y = 0; y < H; y++) {
    // LovyanGFX speichert die Bytes vertauscht
    for (int x = 0; x < W; x++) row[x] = __builtin_bswap16(fb[y * W + x]);
    server.sendContent((const char*)row, sizeof(row));
  }
}

String ago(uint32_t at) {
  if (!at) return "nie";
  uint32_t s = (millis() - at) / 1000;
  if (s < 120) return "vor " + String(s) + " s";
  if (s < 7200) return "vor " + String(s / 60) + " min";
  return "vor " + String(s / 3600) + " h";
}

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_SW: return "Neustart durch die Software";
    case ESP_RST_PANIC: return "Absturz";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_BROWNOUT: return "Spannungseinbruch";
    case ESP_RST_EXT: return "Reset-Taste";
    default: return "sonstiger";
  }
}

void handleStatus() {
  if (!authorized()) return;
  uint32_t up = millis() / 1000;
  char line[200];
  String b = "<div class=box>";
  snprintf(line, sizeof(line), "Laufzeit: %lu d %lu h %lu min<br>Letzter Start: %s<br>Firmware: %s<br>",
           up / 86400, up / 3600 % 24, up / 60 % 60, resetReason(), FIRMWARE_VERSION);
  b += line;
  snprintf(line, sizeof(line), "Interner Speicher: %u KB frei (niedrigster Wert %u KB)<br>PSRAM: %u KB frei<br>",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024, heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024,
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
  b += line;
  snprintf(line, sizeof(line), "WLAN: %s, Signal %d dBm, IP %s", WiFi.SSID().c_str(), WiFi.RSSI(),
           WiFi.localIP().toString().c_str());
  b += line;
  b += "</div><h2>Datenquellen</h2><div class=box>";
  for (int i = 0; i < fetch::SOURCES; i++) {
    const fetch::Source& src = fetch::source(i);
    b += "<b>" + String(src.name) + "</b>: zuletzt erfolgreich " + ago(src.lastOk);
    if (src.lastTry && !src.ok) b += ", <span style=color:#ff8a65>letzter Versuch fehlgeschlagen</span>";
    if (src.failures) b += " <small>(" + String(src.failures) + " Fehler seit dem Start)</small>";
    b += "<br>";
  }
  b += "</div><h2>Log</h2><div class=box><pre style='white-space:pre-wrap;font-size:.8em;margin:0'>";
  String log;
  logLines([](const char* l, void* ctx) { *static_cast<String*>(ctx) += esc(l) + "\n"; }, &log);
  b += log;
  b += "</pre></div><p><a href=/status>Neu laden</a> · <a href=/>Zurück</a></p>";
  server.send(200, "text/html; charset=utf-8", page("Status", b));
}

void handleUpdatePage() {
  if (!authorized()) return;
  server.send(200, "text/html; charset=utf-8",
              page("Firmware-Update",
                   "<p>Installiert: " FIRMWARE_VERSION ". Neue Versionen gibt es unter "
                   "<a href=https://github.com/DG1001/wetteranzeige-esp32/releases>GitHub-Releases</a>.</p>"
                   "<p>Datei <code>firmware.bin</code> (aus dem Release bzw. aus <code>.pio/build/esp32-8048s043/</code>) "
                   "auswählen; die Einstellungen bleiben erhalten.</p>"
                   "<form method=post action=/update enctype=multipart/form-data><input type=file name=firmware "
                   "accept=.bin><button>Hochladen</button></form><p><a href=/>Zurück</a></p>"));
}

void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (!server.authenticate("admin", settings.pin)) return;
  if (up.status == UPLOAD_FILE_START) {
    fetch::pause(true);
    logf("Update: %s", up.filename.c_str());
    ui::status("Firmware-Update", "Bitte nicht ausschalten …");
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    esp_task_wdt_reset();
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

void handleUpdateDone() {
  if (!authorized()) return;
  bool ok = !Update.hasError();
  server.send(200, "text/html; charset=utf-8",
              page(ok ? "Update erfolgreich" : "Update fehlgeschlagen",
                   ok ? "Die Anzeige startet neu." : String("Fehler: ") + Update.errorString()));
  delay(500);
  if (ok) ESP.restart();
  fetch::pause(false);
}

// ---------------------------------------------------------------- Einrichtung

void handlePortalRoot() {
  String b = "<p>Wähle das WLAN, mit dem sich die Anzeige verbinden soll.</p><form method=post action=/speichern>";
  b += wifiFields();
  b += "<label>Ort</label><input name=city value=\"" + esc(settings.city) + "\" placeholder='z. B. Künzelsau'>";
  b += "<button>Speichern und verbinden</button></form>";
  server.send(200, "text/html; charset=utf-8", page("Wetteranzeige einrichten", b));
}

void handlePortalSave() {
  String ssid = server.arg("ssid"), city = server.arg("city");
  ssid.trim();
  city.trim();
  if (ssid.length()) strlcpy(settings.ssid, ssid.c_str(), sizeof(settings.ssid));
  if (server.arg("password").length()) strlcpy(settings.password, server.arg("password").c_str(), sizeof(settings.password));
  if (city != settings.city) {
    strlcpy(settings.city, city.c_str(), sizeof(settings.city));
    settings.lat = settings.lon = 0;  // wird nach dem Verbinden über den Namen gesucht
  }
  saveSettings();
  server.send(200, "text/html; charset=utf-8",
              page("Gespeichert", "Die Anzeige startet neu und verbindet sich mit „" + esc(settings.ssid) + "“."));
  delay(800);
  ESP.restart();
}

// Betriebssysteme prüfen mit festen Adressen, ob Internet da ist; die Umleitung öffnet das Portal
void handlePortalRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// Routen nur einmal registrieren; die Handler unterscheiden zwischen Einrichtung und Normalbetrieb
void setupServer() {
  static bool done = false;
  if (done) return;
  done = true;
  server.on("/", HTTP_GET, [] { portal ? handlePortalRoot() : handleRoot(); });
  server.on("/speichern", HTTP_POST, [] { portal ? handlePortalSave() : handlePortalRedirect(); });
  server.on("/einstellungen", HTTP_POST, [] { portal ? handlePortalRedirect() : handleSave(); });
  server.on("/foto", HTTP_GET, [] { portal ? handlePortalRedirect() : handlePhoto(); });
  server.on("/foto.bmp", HTTP_GET, [] { portal ? handlePortalRedirect() : handleBitmap(); });
  server.on("/status", HTTP_GET, [] { portal ? handlePortalRedirect() : handleStatus(); });
  server.on("/update", HTTP_GET, [] { portal ? handlePortalRedirect() : handleUpdatePage(); });
  server.on("/update", HTTP_POST, [] { portal ? handlePortalRedirect() : handleUpdateDone(); },
            [] { if (!portal) handleUpdateUpload(); });
  server.onNotFound([] {
    if (portal) {
      handlePortalRedirect();
    } else {
      server.send(404, "text/plain", "Nicht gefunden");
    }
  });
  server.begin();
}

}  // namespace

void beginStation(const Model& m, void (*onChange)(const Changes&)) {
  model = &m;
  changed = onChange;
  scanNetworks();
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);

  setupServer();

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(settings.pin);
  ArduinoOTA.setMdnsEnabled(false);  // mDNS läuft schon
  ArduinoOTA.onStart([] {
    fetch::pause(true);
    logf("Update über ArduinoOTA beginnt");
    ui::status("Firmware-Update", "Bitte nicht ausschalten …");
  });
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { esp_task_wdt_reset(); });
  ArduinoOTA.onError([](ota_error_t e) {
    logf("Update über ArduinoOTA fehlgeschlagen (%d)", (int)e);
    fetch::pause(false);
  });
  ArduinoOTA.begin();
  logf("Webseite: http://%s.local/ bzw. http://%s/", HOSTNAME, WiFi.localIP().toString().c_str());
}

void beginPortal() {
  // mDNS und OTA greifen sonst noch auf die abgebaute WLAN-Schnittstelle zu (Absturz in mdns)
  if (model) {
    ArduinoOTA.end();
    MDNS.end();
  }
  portal = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  scanNetworks();
  WiFi.softAP(AP_NAME, settings.pin);
  dns.start(53, "*", WiFi.softAPIP());
  setupServer();
  logf("Einrichtungs-WLAN gestartet");
}

void loop() {
  if (portal) dns.processNextRequest();
  server.handleClient();
  if (!portal) ArduinoOTA.handle();
}

}  // namespace web
