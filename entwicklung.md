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

## 2026-10-19: Radar, Warnungen, Animationen, Detailseiten, Luft & Pollen, Himmel

Gewünscht waren: animiertes Regenradar, Unwetterwarnungen, animierte Wettersymbole, Detailseiten per Touch, Luftqualität/UV/Pollen, Sonnenbogen und Mondphase sowie ein Hintergrund, der Tageszeit und Wetter folgt.

### Datenquellen: Entscheidungen
- **Radar vom DWD-Kartendienst (WMS)** statt Kacheldiensten: Ausschnitt und Bildgröße frei wählbar, ein Bild je Zeitpunkt, mit Transparenz. Vorher am Mac geprüft: `Niederschlagsradar` liefert Messungen der Vergangenheit **und** eine Vorhersage bis 2 Stunden, je etwa 9 KB PNG.
- **Hintergrundkarte** aus drei DWD-Ebenen in **einer** Anfrage (Relief, Flüsse, Landesgrenzen), auf dem Gerät abgedunkelt. Die Kreisebene war mit grüner Füllung unbrauchbar, die Ebene „Basislayer“ gibt es nicht. Städte werden deshalb selbst beschriftet.
- **Warnungen über WFS mit serverseitigem Punktfilter** statt der bundesweiten JSON-Datei (`warnings.json` kann bei Unwetterlagen sehr groß werden). Antwort unter 1 KB. Getestet mit Norden und Helgoland, die zu der Zeit Warnungen hatten.
- **Luftqualität und Pollen von Open-Meteo**, ohne Schlüssel und im selben Stil wie das Wetter
- **Wetterabfrage erweitert:** gestern (für den Vergleich der Tageslänge), 7 Tage, stündliche Details, 15-Minuten-Regen für den Regenhinweis
- **Mondphase lokal berechnet** (mittlerer Mondlauf), statt eine weitere Datenquelle zu nutzen

### Oberfläche: Entscheidungen
- **Seiten per Wischen** (Übersicht, Radar, 48 Stunden, Himmel & Luft) und Unterseiten per Antippen (Tag, Warnungen). Die Übersicht blieb fast unverändert, das Warnsymbol nutzt den freien Platz in der Kopfzeile.
- **Animation nur im animierten Bereich:** Nach jedem vollständigen Bild wird es in den zweiten Bildspeicher kopiert. Danach erneuert jedes Animationsbild nur den Ausschnitt des Symbols bzw. die Radarkarte. Ein komplettes Neuzeichnen 20-mal pro Sekunde wäre zu teuer gewesen.
- **Radarbilder als PNG im Speicher, dekodiert beim Anzeigen.** Dekodiert hätten 19 Bilder rund 12 MB gebraucht.
- **Schrittweises Laden** (ein Abruf je Schleifendurchlauf), damit Touch und Animation bedienbar bleiben
- **Schriften auf Latin-1 und typografische Zeichen erweitert**, damit DWD-Texte („“ – …) vollständig erscheinen. Dazu kam eine fette 22-px-Schrift für Überschriften.
- **Dithering** für die Himmelsverläufe: In RGB565 waren deutliche Farbstufen zu sehen.

### Werkzeug: Bildschirmfotos über die serielle Schnittstelle
Damit das Layout ohne Fotos vom Nutzer geprüft werden kann, gibt es Testbefehle (`seite`, `tipp`, `wisch`, `warnung`, `foto`) und `tools/screenshot.py`. Probleme dabei:
- Mit 921600 Baud gingen Daten verloren, deshalb 460800.
- Ohne Absprache nach dem Umschalten der Baudrate fehlten Bytes am Anfang, die Bilder waren verschoben und farblich verfälscht. Jetzt wartet das Board auf ein „G“ vom Rechner.
- Unter macOS startet das Board beim Öffnen des Ports meist neu (CH340, DTR/RTS), auch wenn DTR/RTS vorher gesetzt werden. Während der Entwicklung hielt deshalb ein Hintergrundprozess den Port offen.
- Ein Testbild-Befehl wurde ausprobiert und wieder entfernt: Der nächste Minutenwechsel hat es überzeichnet.

### Probleme und Lösungen
| Problem | Ursache | Lösung |
|---|---|---|
| Uhr zeigte „--:--“, obwohl die Zeit gestellt war | `getLocalTime(…, 0)` vergleicht zwei `millis()`-Werte; vermutlich lag durch den häufigen Display-Interrupt eine Millisekunde dazwischen | Eigene Funktion `localNow()` mit `time()`/`localtime_r()` |
| Wochentag überlappte Wettersymbole (48 Stunden) | Beschriftung im Diagramm | In die Titelzeile, am rechten Rand links von der Linie |
| Faktenzeile der Tagesansicht zu breit, „km/h“ in der Uhrzeit-Zeile | Layout | Kürzerer Text, Einheiten an die Zeilenbeschriftung, Zeile „Uhr“ |
| Schadstoffzeile lief über den Rand | Einheit am Zeilenende | Einheit in die Titelzeile |
| **Einzelne Pixel flackerten am linken Rand** (Nutzer) | Mit den Animationen ist viel mehr PSRAM-Verkehr da, der Bounce Buffer wurde nicht immer rechtzeitig gefüllt | Weniger Animationslast (12 statt 20 Bilder/s, Radar 2 Bilder/s) und längere Austastlücke, siehe unten |
| **Bild um einige Zeilen nach oben verschoben, obere Zeilen unten sichtbar** (Nutzer) | Der Treiber startet die DMA in jedem VSYNC neu (`CONFIG_LCD_RGB_RESTART_IN_VSYNC`). Mit nur 4 Zeilen Back Porch war der Neustart unter Last nicht rechtzeitig fertig. Der Versuch mit 20 statt 10 Zeilen Bounce Buffer machte es schlimmer, weil das Füllen beim Neustart länger dauert. Mit 12 Zeilen Back Porch blieben noch 2–3 Zeilen Versatz. | Senkrechte Austastlücke auf typische Werte: Back Porch 23, Front Porch 22, Bounce Buffer wieder 10 Zeilen, Pixeltakt 16 MHz (etwa 36 Bilder/s). Danach kein Flackern und keine Verschiebung mehr, bestätigt vom Nutzer. |
| Ein Build dauerte etwa 15 Minuten | pioarduino hat die Arduino-Bibliotheken erneut gebaut, Auslöser unklar | beobachten |

### Noch nicht am Gerät gesehen
Während der Entwicklung war es trocken. Deshalb sind Radarbilder mit Niederschlag, der Regenhinweis bei Regen und die Animation von Regen und Schnee im großen Symbol am Display noch nicht beobachtet worden. Auch die Dämmerungsfarben wurden nur im Code geprüft.

## Offene Punkte und Ideen
- Radar, Regenhinweis und Regenanimation bei echtem Regen prüfen
- Dämmerungsfarben am Gerät prüfen und bei Bedarf anpassen
- Ort und WLAN ohne Neukompilieren ändern, z. B. über ein Einrichtungsportal oder eine Datei auf der SD-Karte
- Updates über WLAN (OTA)
- Nachtmodus mit nur großer Uhr oder ganz abgeschaltetem Display
- Zertifikate prüfen, statt `setInsecure()` zu verwenden
