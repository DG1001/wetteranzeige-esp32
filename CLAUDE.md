# Wetteranzeige ESP32-8048S043

## Dokumentation mitpflegen

Bei jeder Änderung am Projekt die drei Dokumente auf Deutsch aktualisieren:

- `entwicklung.md`: Verlauf mit Datum, Entscheidungen samt Begründung, Probleme mit Ursache und Lösung, auch verworfene Versuche. Neue Einträge unten anfügen, offene Punkte pflegen.
- `technisch.md`: Hardware, Pins, Treiber, Konfiguration, Datenquellen, Formate. Muss zum aktuellen Code passen.
- `fachlich.md`: was die Anzeige zeigt und wie sie sich verhält, aus Sicht des Nutzers

Die README nur anpassen, wenn sich Einrichtung, Build oder Struktur ändern.

## Bauen und Hochladen

- PlatformIO liegt unter `~/.platformio/penv/bin/pio` (nicht im PATH).
- `pio run -t upload --upload-port /dev/cu.usbserial-210`. Die Portnummer kann sich ändern, also mit `ls /dev/cu.usbserial*` prüfen.
- Serielle Ausgabe mit 115200 Baud. Beim Mitlesen keine IP-Adressen oder WLAN-Daten ausgeben.

## Grenzen der Hardware

- Pixeltakt nicht unter 14 MHz, sonst zeigt das Panel seinen Selbsttest.
- Keine großen PSRAM-Kopien am RGB-Treiber vorbei. Gezeichnet wird in den unsichtbaren Bildspeicher, danach `lcd::show()`.
- `include/secrets.h` nie einchecken oder ausgeben.
