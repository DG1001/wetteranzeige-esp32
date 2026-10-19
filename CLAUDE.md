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
- Layout prüfen mit Bildschirmfotos: Testbefehle `seite`, `tipp`, `wisch`, `warnung`, `foto` (siehe `technisch.md`) und `tools/screenshot.py`. Das Öffnen des Ports startet das Board meist neu; für mehrere Fotos den Port in einem Hintergrundprozess offen halten.
- Ein Upload braucht den Port exklusiv, ein offener Monitor oder Hintergrundprozess muss vorher beendet werden.

## Grenzen der Hardware

- Nicht unter etwa 33 Bilder/s gehen (bei 12 MHz und 29 Bildern/s zeigt das Panel seinen Selbsttest).
- Die senkrechte Austastlücke (Back Porch 23, Front Porch 22) nicht verkürzen und den Bounce Buffer nicht vergrößern, sonst verschiebt sich das Bild unter Last um einige Zeilen.
- PSRAM-Last klein halten: Gezeichnet wird in den unsichtbaren Bildspeicher, danach `lcd::show()`. Animationen erneuern nur ihren Ausschnitt und laufen mit höchstens etwa 12 Bildern/s.
- `include/secrets.h` nie einchecken oder ausgeben.
