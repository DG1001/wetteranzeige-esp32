# Technische Details

## Hardware

Sunton **ESP32-8048S043** (Platinenaufdruck „…F1 V1.2“, Variante mit kapazitivem Touch)

| Baugruppe | Details |
|---|---|
| Modul | ESP32-S3-WROOM-1 **N16R8**: 16 MB Flash (QIO), 8 MB Octal-PSRAM |
| Display | 4,3", 800×480, 16-Bit-RGB-Schnittstelle (RGB565) |
| Touch | Goodix GT911 über I²C, auf **480×272** eingestellt (nicht 800×480) |
| USB | USB-C mit Seriell-Wandler CH340, **keine CC-Widerstände** (siehe [README](README.md)) |
| Sonstiges | microSD-Steckplatz, Tasten BOOT und RST |

### Pinbelegung

| Funktion | GPIO |
|---|---|
| R0–R4 | 45, 48, 47, 21, 14 |
| G0–G5 | 5, 6, 7, 15, 16, 4 |
| B0–B4 | 8, 3, 46, 9, 1 |
| HSYNC / VSYNC / DE / PCLK | 39 / 41 / 40 / 42 |
| Hintergrundbeleuchtung (PWM) | 2 |
| Touch SDA / SCL / RST | 19 / 20 / 38 (INT nicht genutzt) |

### Display-Timing (`src/lcd.cpp`)

| Parameter | Wert |
|---|---|
| Pixeltakt | 16 MHz, Daten auf fallender Flanke (`pclk_active_neg`) |
| HSYNC: Pulsbreite / Back Porch / Front Porch | 4 / 16 / 8 |
| VSYNC: Pulsbreite / Back Porch / Front Porch | 4 / **23 / 22** |

Ergibt 828 × 529 Takte je Bild, also etwa 36 Bilder pro Sekunde.

- **Lange senkrechte Austastlücke:** Der Treiber startet die DMA in jedem VSYNC neu (`CONFIG_LCD_RGB_RESTART_IN_VSYNC`) und füllt dabei unter Umständen den ersten Bounce Buffer aus dem PSRAM. Mit 4 bzw. 12 Zeilen Back Porch reichte die Zeit unter Last nicht, das Bild war um einige Zeilen verschoben und flackerte am linken Rand. Siehe [entwicklung.md](entwicklung.md).
- **Bei 12 MHz (etwa 29 Bilder/s) erkennt das Panel kein gültiges Signal** und zeigt seinen Selbsttest. Unter etwa 33 Bildern/s besser nicht gehen.

## Software-Plattform

- **pioarduino** `55.03.312-1`: Arduino-Core 3.3.12 auf ESP-IDF 5.5.5. Die offizielle PlatformIO-Plattform unterstützt Arduino 3.x nicht.
- Board-Definition `esp32-s3-devkitc-1` mit `memory_type = qio_opi`, Partitionstabelle `default_16MB.csv`. Die Datei muss im Projektordner liegen, sonst findet der Hybrid-Build sie nicht.
- Bibliotheken: LovyanGFX (nur zum Zeichnen und PNG-Dekodieren, nicht als Display-Treiber) und ArduinoJson 7

### Eigene ESP-IDF-Einstellungen (`custom_sdkconfig`)

| Option | Zweck |
|---|---|
| `CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y` | Laut Espressif nötig für den Bounce Buffer, sonst kann das Bild wandern |
| `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` | Code und Konstanten laufen aus dem PSRAM. Flash-Schreibzugriffe (WLAN, NVS) sperren dann den Cache nicht mehr. |
| `CONFIG_LCD_RGB_ISR_IRAM_SAFE=y` | Der Display-Interrupt läuft auch während Flash-Zugriffen weiter |

`custom_sdkconfig` löst einen **Hybrid-Build** aus: pioarduino lädt ESP-IDF und baut die Arduino-Bibliotheken neu. Das dauert 10 bis 20 Minuten und **ändert das gemeinsame Paket** `~/.platformio/packages/framework-arduinoespressif32-libs`, was andere Arduino-3-Projekte auf demselben Rechner betrifft. Einmal wurde der Neubau ohne erkennbaren Grund erneut ausgelöst (Build dauerte etwa 15 Minuten). Erzeugte Dateien (`sdkconfig.*`, `managed_components/`, `.dummy/`) stehen in `.gitignore`.

## Aufbau der Software

| Datei | Aufgabe |
|---|---|
| `main.cpp` | Ablauf: WLAN oder Einrichtungs-Hotspot, Abrufplan, Gesten, Nachtmodus, Seitenwechsel, Neuzeichnen und Animation, Testbefehle |
| `model.h` | Gemeinsamer Zustand: Daten, aktuelle Seite, Status |
| `settings.*` | Einstellungen im NVS, Vorgaben beim ersten Start, Nachtzeitraum |
| `web.*` | Webseite, Captive Portal, Firmware-Update im Browser, ArduinoOTA, mDNS |
| `outdoor.*` | Bewertung der Stunden, bestes Zeitfenster, Hinweise |
| `lcd.*` | RGB-Treiber, zwei Bildspeicher, Teilbereiche sichern und wiederherstellen, Hintergrundbeleuchtung |
| `touch.*` | GT911 und Gestenerkennung |
| `net.*` | HTTP(S)-Abrufe für JSON und Binärdaten (PSRAM) |
| `weather.*` | Open-Meteo-Vorhersage, Regenhinweis |
| `air.*` | Open-Meteo-Luftqualität und Pollen |
| `warnings.*` | DWD-Warnungen (WFS) |
| `radar.*` | DWD-Radarkarte und -bilder (WMS), Projektion, Städteliste |
| `astro.*` | Mondphase |
| `theme.*` | Himmelsfarben nach Tageszeit und Wetter |
| `icons.*` | Wettersymbole, statisch und animiert |
| `ui.*` | Seitensteuerung: Neuzeichnen, Animation, Antippen auswerten, Radaransicht |
| `ui_common.*` | Schriften, Text mit Umbruch, Hintergrund mit Dithering, Glas-Karten, Kopfzeile, Temperaturkurve |
| `ui_pages.*` | Übersicht, Draußen, 48 Stunden, Tagesansicht, Himmel & Luft, Warnungen, Nacht, Info |

## Einstellungen (`src/settings.cpp`)

- Gespeichert als ein Block (`struct Settings`) im NVS unter dem Namensraum `wetter`, mit Versionsnummer. Passen Version oder Größe nicht, werden die Vorgaben neu angelegt. **Wird `Settings` geändert, `VERSION` erhöhen.**
- Vorgaben beim ersten Start: WLAN aus `include/secrets.h` (optional), Ort und Helligkeit aus `include/config.h`, Nachtmodus aus (22:30–6:30, gedimmte Uhr, Helligkeit 12), zufällige 8-stellige PIN (`esp_random`).
- Ort: Name und Koordinaten. Sind die Koordinaten 0/0, wird der Name per Open-Meteo-Geocoding gesucht. Ohne Namen wird der Ort über die IP-Adresse bestimmt.

## WLAN, Webseite und Updates (`src/web.cpp`)

- **Normalbetrieb:** `WebServer` auf Port 80, mDNS-Name `wetteranzeige`. Alle Seiten mit HTTP-Basic-Auth (Benutzer `admin`, Passwort = PIN).
  - `GET /`: Status und Formular; `POST /einstellungen` speichert. Danach baut ein Callback in `main.cpp` den Ort neu auf (Radar, Abrufe) und setzt die Helligkeit.
  - `GET /foto.bmp`: BMP mit 16 Bit (BI_BITFIELDS, RGB565, von oben nach unten), zeilenweise aus dem sichtbaren Bildspeicher, Bytes getauscht. Etwa 3 s für 768 KB.
  - `GET/POST /update`: Firmware-Upload mit der `Update`-Bibliothek, danach Neustart
  - Ändert sich das WLAN, startet das Board neu.
- **ArduinoOTA** (Port 3232, Passwort = PIN) für Uploads aus der Entwicklungsumgebung:
  `python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py -i wetteranzeige.local -a PIN -f .pio/build/esp32-8048s043/firmware.bin -r`
- **Einrichtungs-Hotspot** (Captive Portal): `WIFI_AP_STA`, SSID `Wetteranzeige`, WPA2 mit der PIN (deshalb genau 8 Ziffern), `DNSServer` beantwortet alle Namen mit 192.168.4.1, unbekannte Pfade werden dorthin umgeleitet, damit sich das Portal auf Handys von selbst öffnet. Gestartet wird er, wenn kein WLAN eingerichtet ist oder die Verbindung beim Start nicht innerhalb von 20 s klappt.
  - Ohne verbundenen Client wird alle 30 s das gespeicherte WLAN versucht und bei Erfolg neu gestartet; nach 5 Minuten ohne Client ebenfalls Neustart.
  - Vor dem Umschalten aus dem Normalbetrieb werden mDNS und ArduinoOTA beendet. Sonst stürzt die mDNS-Bibliothek ab (`join_group` auf eine abgebaute Netzwerkschnittstelle).
  - Die Routen werden nur einmal registriert und unterscheiden im Handler zwischen Einrichtung und Normalbetrieb. Doppelt registrierte Routen würden sonst die falsche Seite liefern.
- Die Partitionstabelle `default_16MB.csv` hat zwei App-Partitionen zu je 6,25 MB, wie für Updates nötig.

## Nachtmodus (`src/main.cpp`)

- `inNightWindow(minute)` prüft den Zeitraum, auch über Mitternacht. Nachts ist die Seite `Night` aktiv: schwarz, große Uhr in gedämpftem Grau, Temperatur, gegebenenfalls Warnung. Helligkeit `brightClock` oder 0 bei „Display aus“.
- Jede Geste weckt nachts für 60 s (`wakeUntil`) und wird nicht weiter ausgewertet. Danach geht die Anzeige zurück in den Nachtmodus.
- Keine Animation auf der Nachtseite, neu gezeichnet wird einmal pro Minute.

## Draußen (`src/outdoor.cpp`)

- Bewertung je Stunde, 0 bis 100: Basis 100, minus 0,8 × Regenwahrscheinlichkeit, −50 bei Regen ab 0,1 mm, Kälte −2,5 je Grad unter 15° (höchstens −35), Hitze −5 je Grad über 26° (höchstens −50), Wind −1,5 je km/h über 20, −20 bei Böen über 45 km/h, Gewitter −80, Schnee −30, Nebel −15, bedeckt −8, Sonne (Code 0/1) +5. Nachtstunden ergeben 0.
- Zeitfenster: Schwelle = max(50, Tagesbestwert − 12). Gewählt wird der zusammenhängende Lauf über der Schwelle mit der größten Summe (Wert − Schwelle + 10). Tag: heute, wenn noch mindestens 2 Stunden Tageslicht bleiben, sonst morgen.
- Hinweise: siehe [fachlich.md](fachlich.md), ausgewertet werden die nächsten 24 Stunden ab der aktuellen Stunde. Ausgewertet wird nach jedem Wetterabruf.
- Wechselnde Zeile der Übersicht (`ui.cpp`): Regenhinweis, beste Zeit und Hinweise ab Wichtigkeit 5, Wechsel alle 6 s. Weil die Animation zwischen zwei Bildspeichern umschaltet, wird ein neuer Eintrag in **beide** Speicher gezeichnet (`tickerPending = 2`), sonst flackert die Zeile zwischen altem und neuem Text. Der Hintergrund der Zeile wird wie beim Wettersymbol gesichert und wiederhergestellt.

## Display-Ansteuerung (`src/lcd.cpp`)

Treiber ist `esp_lcd_new_rgb_panel` aus ESP-IDF:

- **Bounce Buffer** (`bounce_buffer_size_px = 800 × 10`): Zwei Puffer für je 10 Zeilen im internen RAM. Die CPU kopiert per Interrupt aus dem PSRAM nach, die DMA des Displays liest nur aus dem internen RAM. Größere Puffer (20 Zeilen) waren schlechter, weil das Füllen beim Bildwechsel länger dauert.
- **Zwei Bildspeicher** im PSRAM (`num_fbs = 2`, je 768 KB):
  1. LovyanGFX zeichnet über ein `LGFX_Sprite` mit fremdem Puffer (`setBuffer`) in den unsichtbaren Speicher.
  2. `lcd::show()` ruft `esp_lcd_panel_draw_bitmap` mit diesem Speicher auf. Der Treiber erkennt seinen eigenen Bildspeicher, kopiert nichts und schaltet beim nächsten Bildwechsel um.
  3. Danach wird auf den Callback `on_frame_buf_complete` gewartet (Semaphore, höchstens 100 ms).
- **Animation ohne Komplettneuzeichnen:** Nach einem vollständigen Bild kopiert `lcd::syncBack()` das sichtbare Bild in den anderen Speicher. Danach unterscheiden sich beide nur im animierten Bereich, und jedes Animationsbild erneuert nur diesen:
  - Wettersymbol: Ausschnitt 200 × 182 Pixel. `saveRect` sichert den Hintergrund einmal, `restoreRect` stellt ihn vor jedem Bild wieder her.
  - Radar: der Kartenbereich 800 × 398 aus der gespeicherten Karte
- **Byte-Reihenfolge:** LovyanGFX speichert RGB565 mit vertauschten Bytes: erstes Byte `RRRRRGGG`, zweites Byte `GGGBBBBB`. Der RGB-Treiber erwartet die umgekehrte Reihenfolge. Statt jedes Pixel umzudrehen, sind die Datenleitungen in `DATA_PINS` passend angeordnet: G3 G4 G5 R0–R4 B0–B4 G0 G1 G2.
- **Hintergrundbeleuchtung:** LEDC-PWM mit 20 kHz und 8 Bit

**PSRAM-Last begrenzen:** Das Display liest ständig etwa 26 MB/s aus dem PSRAM (über den Bounce Buffer). Große Kopien im PSRAM konkurrieren damit. Deshalb läuft die Symbolanimation mit etwa 12 Bildern/s (`ANIMATION_MS = 80`) und das Radar mit 2 Bildern/s (`FRAME_S = 0.5`).

## Touch und Gesten (`src/touch.cpp`)

- Reset über GPIO 38, danach wird die Adresse gesucht: erst 0x5D, dann 0x14. Gefunden wurde 0x5D.
- Die Auflösung wird aus den Registern `0x8048`–`0x804B` gelesen (480×272) und auf 800×480 hochgerechnet.
- Abfrage: Statusregister `0x814E` (Bit 7 = neue Daten, Bits 0–3 = Anzahl der Punkte), erster Punkt ab `0x8150`, danach Status auf 0 zurücksetzen. Eine Meldung mit 0 Punkten bedeutet „losgelassen“, als Rückfall gilt ein Finger nach 300 ms ohne Meldung als losgelassen.
- Gesten beim Loslassen: **Wischen** bei mindestens 90 Pixeln waagerecht und mehr als doppelt so viel waagerecht wie senkrecht, **Tippen** bei höchstens 30 Pixeln Bewegung. Die Position ist der Startpunkt.
- **Langes Drücken:** 2 s ohne Bewegung (höchstens 30 Pixel). Es wird schon während der Berührung gemeldet, das spätere Loslassen löst dann keine weitere Geste aus.

## Zeichnen

- **Hintergrund mit Dithering** (`ui_common.cpp`): Die Himmelsverläufe sind dunkel und flach, in RGB565 entstehen sonst sichtbare Stufen. `ditherRow` rastert jede Zeile mit einer 4×4-Bayer-Matrix und schreibt direkt in den Bildspeicher, mit einem Muster aus 4 Pixeln je Zeile.
- **Glas-Karten:** abgerundetes Rechteck in der Mittelfarbe (geglättete Ecken), danach zeilenweise mit dem getönten Verlauf überschrieben (ebenfalls mit Dithering). Tagsüber dunkler getönt (Alpha 60), nachts heller (Alpha 18).
- **Schriften:** `tools/make_vlw.py` erzeugt kantengeglättete VLW-Schriften aus DejaVu Sans:

  | Name | Schrift | Größe | Zeichen |
  |---|---|---|---|
  | `font_small` | DejaVu Sans | 18 px | Text |
  | `font_text` | DejaVu Sans | 24 px | Text |
  | `font_title` | DejaVu Sans Bold | 22 px | Text |
  | `font_bold` | DejaVu Sans Bold | 30 px | Text |
  | `font_clock` | DejaVu Sans Bold | 52 px | Ziffern |
  | `font_huge` | DejaVu Sans Bold | 112 px | Ziffern |

  „Text“ heißt ASCII plus Latin-1 (U+00A0–U+00FF) plus `–—‘’‚“”„…•€`, „Ziffern“ heißt ` -+0123456789°:.%`.
  VLW-Format: Kopf aus 6 × int32 (Big Endian), dann je Zeichen 7 × int32 (Unicode, Höhe, Breite, Vorschub, Abstand Oberkante–Grundlinie, x-Versatz, Füllfeld), danach 8-Bit-Alpha-Bitmaps. Die Zeichen müssen nach Unicode sortiert sein, weil LovyanGFX binär sucht.
- `loadFont()` legt die Zeichentabelle bei jedem Aufruf neu an. `ui_common.cpp` lädt deshalb nur, wenn sich die Schrift ändert.
- LovyanGFX rendert VLW-Zeichen per `alloca` auf dem Stack, deshalb `SET_LOOP_TASK_STACK_SIZE(32 * 1024)`.
- **Textumbruch** (`wrapText`/`wrapLines`): an Leerzeichen und `\n`, mit Höhenbegrenzung und „…“. Die Warnkarten messen ihre Höhe vorab.
- **Großschreibung des DWD** („WINDBÖEN“) wird für das Warnsymbol in normale Schreibweise umgewandelt. UTF-8 Ä/Ö/Ü wird dabei über das zweite Byte `0xC3 0x80–0x9E` + 0x20 behandelt.
- **Wettersymbole** (`icons.cpp`): aus Kreisen, abgerundeten Rechtecken, breiten Linien und Dreiecken. Mit Zeit `t ≥ 0` bewegt: Tropfen und Flocken fallen periodisch und blenden über `mixColor` mit dem Hintergrund ein und aus. Bei `t < 0` statisch (Kacheln, Diagramme).
- `getLocalTime(…, 0)` aus dem Arduino-Core schlug gelegentlich fehl (Uhr zeigte „--:--“). Vermutlich vergeht zwischen den beiden `millis()`-Aufrufen eine Millisekunde, wenn der Display-Interrupt dazwischenkommt. Stattdessen gibt es `ui::localNow()` mit `time()` und `localtime_r()`.

## Himmelsfarben (`src/theme.cpp`)

Feste Farbpaare (oben/unten) für Nacht klar/bewölkt, Dämmerung, Tag klar/bewölkt/Regen/Gewitter/Schnee. Der Bedeckungsgrad aus dem Wettercode dämpft die Dämmerungsfarben. Innerhalb von 50 Minuten vor und nach Sonnenauf- und -untergang wird linear gemischt: Nacht → Dämmerung → Tag bzw. umgekehrt. Text- und Kartenfarben richten sich danach, ob es eher hell oder dunkel ist.

## Datenquellen

### Open-Meteo Wetter (`weather.cpp`)
`https://api.open-meteo.com/v1/forecast`, kein Schlüssel. Antwort etwa 11 KB.
- `current`: Temperatur, gefühlte Temperatur, Luftfeuchte, Wettercode, Wind, Richtung, Böen, Tag/Nacht, Luftdruck, UV
- `hourly`: Temperatur, Regenwahrscheinlichkeit, Regenmenge, Wettercode, Wind, Böen, Tag/Nacht
- `daily`: Wettercode, Höchst- und Tiefstwert, Regenwahrscheinlichkeit, Regensumme, stärkste Böe, UV-Maximum, Sonnenauf- und -untergang, Tageslänge
- `minutely_15=precipitation&forecast_minutely_15=8`: Regen der nächsten 2 Stunden für den Regenhinweis
- `past_days=1&forecast_days=7`: Tag 0 ist gestern (für „kürzer als gestern“), Tag 1 heute. Die Stundenwerte werden über das Datum den Tagen zugeordnet, weil Tage mit Zeitumstellung 23 oder 25 Stunden haben.
- `timezone=auto`: Ortszeit, `utc_offset_seconds` für NTP

### Open-Meteo Luftqualität (`air.cpp`)
`https://air-quality-api.open-meteo.com/v1/air-quality`: `current` mit european_aqi, pm2_5, pm10, ozone, nitrogen_dioxide; `hourly` mit den Pollenarten für heute. Antwort etwa 3 KB.

### DWD Warnungen (`warnings.cpp`)
WFS `https://maps.dwd.de/geoserver/dwd/ows`, Ebene `dwd:Warnungen_Gemeinden`, gefiltert auf dem Server mit `CQL_FILTER=INTERSECTS(THE_GEOM,POINT(lon lat))` (WFS 1.0.0: Länge zuerst). Mit `propertyName` werden nur die benötigten Felder geholt, ohne Geometrie. Antwort unter 1 KB je Warnung. `SEVERITY` Minor/Moderate/Severe/Extreme ergibt die Stufen 1–4. Zeiten in UTC werden mit dem Versatz von Open-Meteo in Ortszeit umgerechnet.

### DWD Radar (`radar.cpp`)
WMS `https://maps.dwd.de/geoserver/dwd/wms`, Version 1.1.1, `srs=EPSG:4326`. Ausschnitt 300 km breit um den Ort, Höhe passend zu 800 × 398 Pixeln.
- **Karte** (einmal, etwa 130 KB PNG): Ebenen `Natural_Earth_Map,Gewaesser,Laender` mit Stilen `natural_earth_map,fluesse,Laender_weiß` in einer Anfrage. Auf dem Gerät dekodiert und abgedunkelt bzw. leicht blau getönt, dann im PSRAM gehalten (637 KB).
- **Radar** (19 Bilder, je etwa 8–10 KB PNG mit Transparenz): Ebene `Niederschlagsradar` mit `time=…Z`. Die Ebene liefert Messungen der Vergangenheit und die Vorhersage bis 2 Stunden. Bezugszeit ist jetzt minus 10 Minuten, auf 5 Minuten abgerundet. Bilder von −60 bis +120 Minuten in 10-Minuten-Schritten. Gespeichert werden die PNG-Daten, dekodiert wird erst beim Anzeigen (`drawPng` mit Alphakanal).
- Geladen wird schrittweise (ein Teil je Durchlauf der Hauptschleife), zuerst das aktuelle Bild, dann rückwärts die Vergangenheit, dann die Vorhersage.
- Städte: feste Liste (Region Hohenlohe und deutsche Großstädte), nach Wichtigkeit, nur ohne Überlappung beschriftet.
- Die Daten stammen vom Deutschen Wetterdienst, die Quelle wird auf der Karte genannt. Die Reliefkarte stammt von Natural Earth (gemeinfrei).

### ip-api.com (Standort)
Nur wenn `LOCATION_NAME` leer ist: `http://ip-api.com/json/?fields=status,city,lat,lon&lang=de`

### Hinweise zu den Abrufen (`net.cpp`)
- HTTPS ohne Zertifikatsprüfung (`setInsecure`), `useHTTP10(true)` gegen Chunked-Encoding
- Binärdaten (PNG) landen in einem PSRAM-Puffer, der bei Bedarf wächst.
- Die Meldungen `ssl_client … (-76)` im Log sind harmlos: Der Server schließt die Verbindung nach der Antwort.
- Höchstens ein Abruf je Durchlauf der Hauptschleife, damit Touch und Animation nicht lange warten.

## Ablauf (`src/main.cpp`)

1. `setup`: Display, Hintergrundbeleuchtung, Touch, WLAN verbinden (höchstens 20 s)
2. `loop`:
   - Testbefehle von der seriellen Schnittstelle
   - Geste auswerten (Seitenwechsel, Antippen-Ziele über `ui::hit`)
   - ein fälliger Abruf: Wetter, sonst Warnungen, sonst Luft, sonst ein Radarteil
   - bei Minutenwechsel, neuen Daten oder Seitenwechsel komplett neu zeichnen (`ui::render`), sonst alle 80 ms ein Animationsbild (`ui::animate`)
   - nach 3 Minuten ohne Berührung zurück zur Übersicht

## Testbefehle und Bildschirmfotos

Über die serielle Schnittstelle (115200 Baud, eine Zeile je Befehl):

| Befehl | Wirkung |
|---|---|
| `seite N` | Seite N: 0 Übersicht, 1 Draußen, 2 Radar, 3 48 Stunden, 4 Himmel & Luft, 5 Tag, 6 Warnungen, 7 Nacht, 8 Info |
| `tipp X Y` | Antippen simulieren |
| `wisch l` / `wisch r` | Wischen nach links bzw. rechts |
| `pin` | PIN ausgeben, z. B. für Tests der Webseite mit curl |
| `einrichtung` | Einrichtungs-Hotspot starten, die Einstellungen bleiben erhalten |
| `warnung B L` | Warnungen für Breite B und Länge L laden, z. B. für einen Ort mit aktueller Warnung |
| `foto` | Bildschirmfoto: Zeile `FOTO 800 480`, Umschalten auf 460800 Baud, nach einem `G` vom Rechner 768 000 Bytes Rohdaten |

`tools/screenshot.py PORT bild.png [befehl …]` holt ein Foto als PNG (benötigt pyserial und Pillow). Einfacher geht es im Heimnetz über `http://wetteranzeige.local/foto.bmp`. Beim Öffnen des Ports startet das Board unter macOS meist neu (CH340, DTR/RTS). Für mehrere Fotos hintereinander ist daher ein Prozess besser, der den Port offen hält. Mit 921600 Baud gingen Daten verloren.

## Ressourcen

- RAM (statisch): ca. 71 KB von 320 KB; zur Laufzeit etwa 220 KB interner Speicher frei
- Flash: ca. 1,71 MB von 6,25 MB (App-Partition), davon etwa 345 KB Schriften
- PSRAM: 2 × 768 KB Bildspeicher, 637 KB Radarkarte, 73 KB Symbol-Hintergrund, Radar-PNGs (etwa 200 KB), TLS- und JSON-Puffer

## Upload

- **Über WLAN** (bevorzugt): Webseite `/update` oder `espota.py`, siehe oben
- Über USB: `upload_speed = 460800`. Bei 921600 bricht esptool mit „Unable to verify flash chip connection“ ab.
- USB-Seriell über CH340, am Mac `/dev/cu.usbserial-*`
- **USB-Stabilität:** Der CH340 hat sich mehrmals kurz abgemeldet („Device not configured“), jeweils beim Verbindungsaufbau des WLANs oder beim Start des Hotspots, also bei hohem Sendestrom. Vermutlich sind das Spannungseinbrüche über Kabel und Adapter. Danach hing der Treiber am Mac (esptool und pyserial blockierten), bis das Kabel neu eingesteckt wurde. Einmal blieb das Board nach einem abgebrochenen USB-Upload im Bootloader.
