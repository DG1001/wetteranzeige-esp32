# Wetteranzeige ESP32-8048S043

Wetterstation für das Sunton-Board **ESP32-8048S043** (ESP32-S3, 4,3"-Display mit 800×480 Pixeln, kapazitiver Touch).

- Übersicht mit animiertem Wettersymbol, Regenhinweis für die nächsten 2 Stunden, 24-Stunden-Verlauf und 6 Tagen
- Animiertes Regenradar des DWD mit 1 Stunde Vergangenheit und 2 Stunden Vorhersage
- Amtliche Unwetterwarnungen des DWD
- 48-Stunden-Verlauf und Tagesansichten
- Sonnenbogen, Mondphase, UV-Index, Luftqualität und Wasserbilanz für den Garten
- Pollenflug und Biowetter des DWD
- Beste Zeit für draußen mit Hinweisen (Frost, Gewitter, Schirm, UV, Wäsche …)
- Hintergrund, der Tageszeit und Wetter folgt
- Nachtmodus mit gedimmter Uhr oder dunklem Display
- Einrichtung, Einstellungen und Firmware-Updates über WLAN

Bedienung per Wischen und Antippen (siehe [fachlich.md](fachlich.md)). Die Daten kommen über WLAN von [Open-Meteo](https://open-meteo.com) und vom [Deutschen Wetterdienst](https://www.dwd.de), API-Schlüssel sind nicht nötig.

## Voraussetzungen

- [PlatformIO](https://platformio.org) (CLI oder VS-Code-Erweiterung)
- **USB-A-auf-USB-C-Kabel.** Mit einem C-auf-C-Kabel bekommt das Board keinen Strom, weil an der USB-C-Buchse die CC-Widerstände fehlen. Am Mac geht ein A-auf-C-Kabel mit Adapter.
- WLAN mit 2,4 GHz (5 GHz kann der ESP32-S3 nicht)

## Einrichtung

1. Firmware einmal per USB hochladen (siehe unten).
2. Ohne eingerichtetes WLAN öffnet die Anzeige einen Hotspot „Wetteranzeige“. Den QR-Code auf dem Display scannen oder mit dem angezeigten Passwort verbinden, dann WLAN und Ort eintragen.
3. Danach gibt es die Webseite im Heimnetz unter `http://wetteranzeige.local`. Benutzer ist `admin`, die PIN steht am Gerät: **2 Sekunden auf das Display drücken**. Dort lassen sich Ort, DWD-Regionen, Helligkeit, Nachtmodus und WLAN einstellen, ein Bildschirmfoto holen, Firmware hochladen und unter „Status und Log“ der Zustand des Geräts ansehen.

Optional lassen sich WLAN-Daten schon beim Bauen mitgeben. Dazu `include/secrets.h.example` nach `include/secrets.h` kopieren und ausfüllen. Die Datei steht in `.gitignore` und dient nur als Vorgabe beim allerersten Start. Ort und Helligkeit für den ersten Start stehen in `include/config.h`.

## Bauen und Hochladen

```sh
pio run                                          # bauen
pio run -t upload --upload-port /dev/cu.usbserial-XXX   # per USB
pio device monitor                               # serielle Ausgabe (115200 Baud)
```

Über WLAN hochladen, wenn die Firmware schon läuft (PIN siehe oben):

```sh
python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i wetteranzeige.local -a PIN -f .pio/build/esp32-8048s043/firmware.bin -r
```

Alternativ auf der Webseite unter „Firmware-Update“ die Datei `.pio/build/esp32-8048s043/firmware.bin` auswählen.

- Der **erste Build dauert 10 bis 20 Minuten.** pioarduino lädt dabei ESP-IDF herunter und baut die Arduino-Bibliotheken mit eigenen Einstellungen neu (`custom_sdkconfig`). Danach dauert ein Build nur noch Sekunden.
- Per USB wird mit 460800 Baud hochgeladen. Bei 921600 Baud bricht der Upload ab.
- Das Board erscheint am Mac als `/dev/cu.usbserial-*` (USB-Seriell-Wandler CH340).

## Projektstruktur

| Pfad | Inhalt |
|---|---|
| `src/main.cpp` | Ablauf: WLAN, Abrufplan, Gesten, Nachtmodus, Neuzeichnen und Animation, Testbefehle |
| `src/settings.*`, `web.*` | Einstellungen im Flash, Webseite, Einrichtungs-Hotspot, Updates über WLAN |
| `src/outdoor.*` | Beste Zeit draußen und Hinweise |
| `src/lcd.*` | RGB-Display über den ESP-IDF-Treiber (Bounce Buffer, zwei Bildspeicher), Hintergrundbeleuchtung |
| `src/touch.*` | Touch-Controller GT911, Tippen und Wischen |
| `src/fetch.*` | Hintergrund-Task für alle Abrufe |
| `src/weather.*`, `air.*`, `garden.*`, `warnings.*`, `radar.*`, `health.*` | Daten von Open-Meteo und DWD |
| `src/log.*` | Log mit Ringpuffer für die Statusseite |
| `src/astro.*`, `theme.*` | Mondphase, Himmelsfarben |
| `src/ui*.*`, `icons.*` | Seiten, Zeichnen mit LovyanGFX, animierte Wettersymbole |
| `include/fonts/` | Erzeugte Schriften (nicht von Hand bearbeiten) |
| `tools/make_vlw.py` | Erzeugt die Schriften aus DejaVu Sans |
| `tools/screenshot.py` | Holt ein Bildschirmfoto über die serielle Schnittstelle |

## Dokumentation

- [fachlich.md](fachlich.md): was die Anzeige zeigt und wie sie sich verhält
- [technisch.md](technisch.md): Hardware, Treiber, Konfiguration, Datenquellen
- [entwicklung.md](entwicklung.md): Verlauf der Entwicklung mit Entscheidungen und Problemen

## Quellen und Lizenzen

- Wetter, Luftqualität und Pollen: [Open-Meteo](https://open-meteo.com) (CC BY 4.0)
- Radar und Warnungen: [Deutscher Wetterdienst](https://www.dwd.de), Quelle wird auf der Radarseite genannt
- Reliefkarte: Natural Earth (gemeinfrei), über den Kartendienst des DWD

## Schriften

Die Schriften in `include/fonts/` werden aus **DejaVu Sans** erzeugt (Bitstream Vera License, Änderungen von DejaVu gemeinfrei). Neu erzeugen geht mit `python3 tools/make_vlw.py`. Dafür werden Pillow und matplotlib gebraucht, weil matplotlib die DejaVu-Dateien mitliefert.
