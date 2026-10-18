# Wetteranzeige ESP32-8048S043

Wetterstation für das Sunton-Board **ESP32-8048S043** (ESP32-S3, 4,3"-Display mit 800×480 Pixeln, kapazitiver Touch).
Zeigt das aktuelle Wetter, den Verlauf der nächsten 24 Stunden und eine 6-Tage-Vorhersage. Die Daten kommen über WLAN von [Open-Meteo](https://open-meteo.com), ein API-Schlüssel ist nicht nötig.

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
| `src/main.cpp` | Ablauf: WLAN, Abruf-Zeitplan, Touch, Neuzeichnen |
| `src/lcd.*` | RGB-Display über den ESP-IDF-Treiber (Bounce Buffer, zwei Bildspeicher), Hintergrundbeleuchtung |
| `src/touch.*` | Touch-Controller GT911 über I²C |
| `src/weather.*` | Standort und Wetterdaten (Open-Meteo, ip-api.com) |
| `src/ui.*` | Layout und Zeichnen mit LovyanGFX |
| `src/icons.*` | Wettersymbole aus Grundformen |
| `include/fonts/` | Erzeugte Schriften mit Umlauten (nicht von Hand bearbeiten) |
| `tools/make_vlw.py` | Erzeugt die Schriften aus DejaVu Sans |

## Dokumentation

- [fachlich.md](fachlich.md): was die Anzeige zeigt und wie sie sich verhält
- [technisch.md](technisch.md): Hardware, Treiber, Konfiguration, Datenquellen
- [entwicklung.md](entwicklung.md): Verlauf der Entwicklung mit Entscheidungen und Problemen

## Schriften

Die Schriften in `include/fonts/` werden aus **DejaVu Sans** erzeugt (Bitstream Vera License, Änderungen von DejaVu gemeinfrei). Neu erzeugen geht mit `python3 tools/make_vlw.py`. Dafür werden Pillow und matplotlib gebraucht, weil matplotlib die DejaVu-Dateien mitliefert.
