# Entwicklung

Verlauf der Entwicklung mit Entscheidungen und Problemen, neueste Einträge unten.

## 2026-10-18: Inbetriebnahme

### Board wird nicht erkannt
- **Problem:** Am Mac erschien kein serieller Port, im USB-Baum war kein Gerät zu sehen, und das Display blieb dunkel.
- **Untersuchung:** Auf der Platine ist neben R17/R3/R4 (I²C-Pull-ups des Touch-Controllers am Stecker FPC1) ein dunkler Fleck aus angebranntem Flussmittel, und R17 scheint unbestückt. Diese Stelle gehört zum Touch, nicht zu USB, und erklärt den Fehler nicht. Der Touch hat später einwandfrei funktioniert.
- **Ursache:** Es war ein USB-C-auf-C-Kabel. Den Sunton-Boards fehlen die CC-Widerstände an der Buchse, deshalb liefert der Mac keinen Strom.
- **Lösung:** USB-A-auf-C-Kabel mit A-auf-C-Adapter. Danach erschien `/dev/cu.usbserial-210` (CH340).

### Erste Demo
- **Entscheidung:** PlatformIO mit Arduino-Core 2.0.17, der schon installiert war, und LovyanGFX als Grafik- und Displaybibliothek
- Fehler beim Kompilieren: `I2C_NUM_1` ist in dieser Konstellation nicht deklariert. Die Lösung war, den Port als `1` anzugeben.
- **Problem:** Der Upload mit 921600 Baud brach ab („Unable to verify flash chip connection“). Mit **460800 Baud** ging es.
- **Problem:** Der Touch war verschoben. Unten rechts landete der Punkt etwa in der Bildmitte. Der GT911 ist auf **480×272** eingestellt, nicht auf 800×480. Die Lösung war, den Koordinatenbereich auf 480×272 zu setzen und hochzurechnen.

## 2026-10-18: Wetteranzeige

### Entscheidungen
- **Datenquelle Open-Meteo:** braucht keinen API-Schlüssel und keine Anmeldung und hat aktuelle Werte sowie Stunden- und Tagesvorhersage in einem Aufruf.
- **Standort zunächst über die IP-Adresse (ip-api.com),** damit nichts eingetragen werden muss. Ergebnis: Heilbronn. Der Nutzer wollte **Künzelsau**, deshalb ist der Ort jetzt fest in `config.h` eingetragen (49,282 / 9,689). Die automatische Bestimmung bleibt als Rückfall, wenn `LOCATION_NAME` leer ist.
- **WLAN-Daten in `include/secrets.h`** (in `.gitignore`), damit das Passwort nicht im Repository und nicht im Chat landet. Ein Einrichtungsportal wie WiFiManager wurde verworfen, weil es für ein einzelnes Gerät zu aufwändig ist.
- **Eigene Schriften:** LovyanGFX hat nur ASCII-Schriften oder japanische Schriften mit Latin-1. Mit `tools/make_vlw.py` werden deshalb kantengeglättete VLW-Schriften aus DejaVu Sans erzeugt. Pillow und DejaVu, das matplotlib mitbringt, waren auf dem Mac schon vorhanden.
- **Wettersymbole aus Grundformen** statt Bilddateien: skalierbar, kein zusätzlicher Speicher, passt zu den geglätteten Kanten.
- **Ganzer Bildschirm als Sprite im PSRAM:** zeichnen ohne sichtbaren Bildaufbau, danach auf einmal anzeigen.

### Probleme
- LovyanGFX rendert VLW-Zeichen per `alloca` auf dem Stack. Ziffern in 112 px hätten den 8-KB-Loop-Stack gesprengt, deshalb `SET_LOOP_TASK_STACK_SIZE(32 KB)`. Das wurde vorher im Quellcode erkannt, nicht erst durch einen Absturz.
- Im Statustext fehlte das Zeichen „…“ in der Schrift. Es ist jetzt im Zeichensatz enthalten.

## 2026-10-18: Springendes Bild

### Problem
Das Bild **sprang kurz**, verschob sich also für ein Bild seitlich. Ursache: Das RGB-Display liest seinen Bildspeicher laufend per DMA aus dem PSRAM. Große Kopiervorgänge im PSRAM und WLAN-Aktivität bremsen dieses Lesen. Läuft der Puffer der DMA leer, verrutscht die Bildausgabe. LovyanGFX setzt sie beim nächsten VSYNC zurück, deshalb ist der Fehler nur kurz zu sehen.

### Versuche mit Arduino 2.x und dem LovyanGFX-Treiber
| Versuch | Ergebnis |
|---|---|
| Pixeltakt 16 → 14 MHz | weniger Sprünge |
| Beim Minutenwechsel nur die Kopfzeile neu zeichnen statt den ganzen Bildschirm | jede Minute ruhig |
| `WiFi.persistent(false)`: keine Flash-Schreibzugriffe durch das WLAN | Teil der Gesamtlösung |
| Bild in Streifen von 16 Zeilen mit 2 ms bzw. 5 ms Pause kopieren | beim Antippen weiter kleine Sprünge |
| Pixeltakt 12 MHz | **Panel zeigt seinen Selbsttest** (rot, grün, blau, schwarz im Wechsel), zurück auf 14 MHz |

### Recherche
- Die [ESP-FAQ von Espressif zu LCDs](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html) sagt ausdrücklich, dass sich das Problem mit Arduino-Core 2.x (ESP-IDF 4.4) nicht lösen lässt. Nötig sind ESP-IDF ab 5.1 mit **Bounce Buffer**, 64-Byte-Cache-Zeilen und Code aus dem PSRAM.
- In Foren gibt es dasselbe Problem mit genau diesem Board, ohne allgemein gültige Lösung. Häufigster Rat ist ein niedrigerer Pixeltakt.

### Entscheidung: Umstieg auf Arduino-Core 3.x und den ESP-IDF-Treiber
- **pioarduino** `55.03.312-1` (Arduino 3.3.12, ESP-IDF 5.5.5), weil die offizielle PlatformIO-Plattform Arduino 3.x nicht unterstützt
- LovyanGFX nutzt beim RGB-Display eine eigene DMA-Ansteuerung ohne Bounce Buffer, deshalb **steuert jetzt `esp_lcd_new_rgb_panel` das Display an**. LovyanGFX zeichnet nur noch in den Speicher.
- **Zwei Bildspeicher** mit Umschalten beim Bildwechsel statt Kopieren. Das Streifen-Kopieren und das Neuzeichnen nur der Kopfzeile sind damit überflüssig und wurden entfernt.
- Die **Byte-Reihenfolge** von LovyanGFX (RGB565 mit vertauschten Bytes) wird über die Anordnung der Datenleitungen ausgeglichen, statt jedes Pixel umzudrehen.
- Für den **Touch** gibt es einen eigenen GT911-Treiber, weil die LovyanGFX-Touchklassen an ein LovyanGFX-Panel gebunden sind. Er liest die Auflösung aus dem Chip, statt sie fest einzutragen.
- Pixeltakt zurück auf 16 MHz

### Probleme bei der Umstellung
- Der erste Build dauerte rund 15 Minuten, weil pioarduino für `custom_sdkconfig` ESP-IDF herunterlädt und die Arduino-Bibliotheken neu baut.
- „Missing partition table file default_16MB.csv“: Der Hybrid-Build sucht die Datei im Projektordner. Sie ist jetzt dorthin kopiert.
- In Core 3.x bringt `WiFiClientSecure.h` die Klasse `WiFiClient` nicht mehr mit, deshalb ist `#include <WiFi.h>` hinzugekommen.
- `on_bounce_frame_finish` ist veraltet, verwendet wird `on_frame_buf_complete`.

### Ergebnis (2026-10-19)
Die Farben stimmen, und auch beim Antippen springt das Bild nicht mehr. Bestätigt vom Nutzer am Gerät.

## 2026-10-19: Repository und Dokumentation
- Git-Repository angelegt mit README, `fachlich.md`, `technisch.md` und dieser Datei
- Die drei Dokumente werden bei jeder Änderung mitgepflegt (siehe `CLAUDE.md`).

## Offene Punkte und Ideen
- Ort und WLAN ohne Neukompilieren ändern, z. B. über ein Einrichtungsportal oder eine Datei auf der SD-Karte
- Helligkeit zusätzlich nach Uhrzeit steuern, nicht nur nach Tag und Nacht
- Zertifikat von Open-Meteo prüfen, statt `setInsecure()` zu verwenden
- Updates über WLAN (OTA)
