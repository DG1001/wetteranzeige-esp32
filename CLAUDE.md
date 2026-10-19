# Wetteranzeige ESP32-8048S043

## Dokumentation mitpflegen

Bei jeder Änderung am Projekt die drei Dokumente auf Deutsch aktualisieren:

- `entwicklung.md`: Verlauf mit Datum, Entscheidungen samt Begründung, Probleme mit Ursache und Lösung, auch verworfene Versuche. Neue Einträge unten anfügen, offene Punkte pflegen.
- `technisch.md`: Hardware, Pins, Treiber, Konfiguration, Datenquellen, Formate. Muss zum aktuellen Code passen.
- `fachlich.md`: was die Anzeige zeigt und wie sie sich verhält, aus Sicht des Nutzers

Die README nur anpassen, wenn sich Einrichtung, Build oder Struktur ändern.

## Öffentliches Repository

Das Repository ist öffentlich: https://github.com/DG1001/wetteranzeige-esp32 (MIT-Lizenz).
- Vor jedem Commit und Push prüfen, dass keine PIN, WLAN-Namen, Passwörter oder lokale IP-Adressen enthalten sind.
- Bildschirmfotos der Info- und der Einrichtungsseite nie einchecken, sie zeigen die PIN.
- Commits laufen mit der GitHub-noreply-Adresse (lokal in `.git/config` gesetzt).

## Bauen und Hochladen

- PlatformIO liegt unter `~/.platformio/penv/bin/pio` (nicht im PATH).
- Hochladen bevorzugt **über WLAN** mit `espota.py` (Befehl in `technisch.md`). Die PIN liefert der serielle Befehl `pin`; sie darf nicht in Dateien im Repository oder in Ausgaben landen.
- Per USB: `pio run -t upload --upload-port /dev/cu.usbserial-210`. Die Portnummer kann sich ändern, also mit `ls /dev/cu.usbserial*` prüfen. Hängt esptool, ist meist der CH340-Treiber am Mac blockiert; dann den Nutzer bitten, das Kabel neu einzustecken.
- Serielle Ausgabe mit 115200 Baud. Beim Mitlesen keine IP-Adressen oder WLAN-Daten ausgeben.
- Layout prüfen mit Bildschirmfotos: Testbefehle `seite`, `tipp`, `wisch`, `warnung`, `foto` (siehe `technisch.md`) und `tools/screenshot.py`. Das Öffnen des Ports startet das Board meist neu; für mehrere Fotos den Port in einem Hintergrundprozess offen halten.
- Ein Upload braucht den Port exklusiv, ein offener Monitor oder Hintergrundprozess muss vorher beendet werden.

## Grenzen der Hardware

- Pixeltakt 22 MHz (50 Bilder/s) beibehalten: Darunter flimmern helle Flächen sichtbar (bei 36 Bildern/s deutlich), bei 29 Bildern/s zeigt das Panel nur noch seinen Selbsttest. Mehr als etwa 22 MHz schafft der PSRAM nicht zuverlässig.
- Die senkrechte Austastlücke (Back Porch 23, Front Porch 22) nicht verkürzen und den Bounce Buffer nicht vergrößern, sonst verschiebt sich das Bild unter Last um einige Zeilen.
- PSRAM-Last klein halten: Gezeichnet wird in den unsichtbaren Bildspeicher, danach `lcd::show()`. Animationen erneuern nur ihren Ausschnitt und laufen mit höchstens etwa 12 Bildern/s.
- `include/secrets.h` nie einchecken oder ausgeben.
- Neue Felder in `struct Settings` nur hinten anfügen, dann bleiben gespeicherte Einstellungen erhalten. `VERSION` nur bei unverträglichen Änderungen erhöhen, denn dann gehen WLAN und PIN verloren.
- Netzwerkzugriffe gehören in den Abruf-Task (`fetch.cpp`), nicht in die Hauptschleife. Das Modell wird nur unter `ModelLock` geändert oder gelesen.
