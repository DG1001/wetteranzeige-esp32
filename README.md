# Wetteranzeige ESP32-8048S043

Wetterstation für das Sunton-Board **ESP32-8048S043** (ESP32-S3, 4,3"-Display mit 800×480 Pixeln, kapazitiver Touch).

- Übersicht mit animiertem Wettersymbol, Regenhinweis für die nächsten 2 Stunden, 24-Stunden-Verlauf und 6 Tagen
- Animiertes Regenradar des DWD mit 1 Stunde Vergangenheit und 2 Stunden Vorhersage
- Amtliche Unwetterwarnungen des DWD
- 48-Stunden-Verlauf und Tagesansichten
- Sonnenbogen, Mondphase, UV-Index, Luftqualität und Pollen
- Hintergrund, der Tageszeit und Wetter folgt

Bedienung per Wischen und Antippen (siehe [fachlich.md](fachlich.md)). Die Daten kommen über WLAN von [Open-Meteo](https://open-meteo.com) und vom [Deutschen Wetterdienst](https://www.dwd.de), API-Schlüssel sind nicht nötig.

## Voraussetzungen

- [PlatformIO](https://platformio.org) (CLI oder VS-Code-Erweiterung)
- **USB-A-auf-USB-C-Kabel.** Mit einem C-auf-C-Kabel bekommt das Board keinen Strom, weil an der USB-C-Buchse die CC-Widerstände fehlen. Am Mac geht ein A-auf-C-Kabel mit Adapter.
- WLAN mit 2,4 GHz (5 GHz kann der ESP32-S3 nicht)

## Einrichtung

1. WLAN-Zugangsdaten eintragen:
   ```sh
   cp include/secrets.h.example include/secrets.h
   # WIFI_SSID und WIFI_PASSWORD in include/secrets.h setzen
   ```
   `include/secrets.h` steht in `.gitignore` und wird nicht eingecheckt.
2. Optional den Ort, das Abrufintervall und die Helligkeit in `include/config.h` anpassen. Voreingestellt ist Künzelsau. Mit leerem `LOCATION_NAME` wird der Ort über die IP-Adresse bestimmt.

## Bauen und Hochladen

```sh
pio run                                          # bauen
pio run -t upload --upload-port /dev/cu.usbserial-XXX
pio device monitor                               # serielle Ausgabe (115200 Baud)
```

- Der **erste Build dauert 10 bis 20 Minuten.** pioarduino lädt dabei ESP-IDF herunter und baut die Arduino-Bibliotheken mit eigenen Einstellungen neu (`custom_sdkconfig`). Danach dauert ein Build nur noch Sekunden.
- Hochgeladen wird mit 460800 Baud. Bei 921600 Baud bricht der Upload ab.
- Das Board erscheint am Mac als `/dev/cu.usbserial-*` (USB-Seriell-Wandler CH340).

## Projektstruktur

| Pfad | Inhalt |
|---|---|
| `src/main.cpp` | Ablauf: WLAN, Abrufplan, Gesten, Neuzeichnen und Animation, Testbefehle |
| `src/lcd.*` | RGB-Display über den ESP-IDF-Treiber (Bounce Buffer, zwei Bildspeicher), Hintergrundbeleuchtung |
| `src/touch.*` | Touch-Controller GT911, Tippen und Wischen |
| `src/weather.*`, `air.*`, `warnings.*`, `radar.*` | Daten von Open-Meteo und DWD |
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
