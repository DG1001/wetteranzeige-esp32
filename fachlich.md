# Fachliche Beschreibung

## Zweck

Eine Wetteranzeige für einen festen Ort, die ohne Bedienung läuft. Sie zeigt auf einen Blick, wie das Wetter jetzt ist, wie es sich in den nächsten Stunden entwickelt und wie die kommenden Tage werden.

## Bildschirmaufbau (800×480)

### Kopfzeile
- **Ort**, links oben. Er ist in `include/config.h` fest eingestellt (Künzelsau) oder wird über die IP-Adresse bestimmt.
- **Datum** ausgeschrieben, z. B. „Sonntag, 18. Oktober 2026“
- **Uhrzeit** groß, rechts oben, im Format HH:MM
- **Status** darunter klein:
  - „Stand 22:40“: Zeitpunkt des letzten erfolgreichen Abrufs
  - „Aktualisiere …“: nach dem Antippen, bis die neuen Daten da sind
  - „Kein WLAN · Stand …“ (orange): WLAN getrennt, es werden die letzten Daten angezeigt
  - „Abruf fehlgeschlagen · Stand …“ (orange): seit mehr als 30 Minuten kein erfolgreicher Abruf

### Aktuelles Wetter
- Großes **Wettersymbol** mit Sonne bzw. Mond je nach Tageszeit
- **Temperatur** groß, auf ganze Grad gerundet
- **Beschreibung**, z. B. „Teilweise bewölkt“
- Kasten mit Details:
  - Gefühlte Temperatur
  - Luftfeuchte in %
  - Wind in km/h mit Himmelsrichtung (N, NO, O, SO, S, SW, W, NW)
  - Luftdruck in hPa (am Standort, nicht auf Meereshöhe umgerechnet)
  - Sonnenaufgang und -untergang des heutigen Tages

### Nächste 24 Stunden
- Temperaturkurve, stündlich, beginnend mit der aktuellen Stunde
- Alle 3 Stunden ein Punkt mit Temperatur und Uhrzeit, der erste mit „Jetzt“ beschriftet
- Blaue Balken: Regenwahrscheinlichkeit pro Stunde (Balkenhöhe entspricht 0 bis 100 %)

### 6-Tage-Vorhersage
- Eine Kachel pro Tag, die erste „Heute“, danach die Wochentage (Mo, Di, …)
- Wettersymbol (immer in der Tagesvariante), Höchst- und Tiefsttemperatur, höchste Regenwahrscheinlichkeit des Tages

## Verhalten

| Situation | Verhalten |
|---|---|
| Start | „Verbinde mit WLAN …“, danach „Lade Wetterdaten …“ |
| WLAN nicht erreichbar | Meldung mit Hinweis auf `include/secrets.h`; die Verbindung wird automatisch weiter versucht |
| Regelmäßig | Wetterabruf alle 10 Minuten, Uhr wird jede Minute aktualisiert |
| Abruf fehlgeschlagen | Neuer Versuch nach 1 Minute; die alten Daten bleiben sichtbar |
| Antippen | Sofortiger neuer Abruf; ein gehaltener Finger löst nur einmal aus |
| Tag/Nacht | Tagsüber blaues Farbschema und Helligkeit 220, nachts dunkles Schema und Helligkeit 60 (Werte in `config.h`). Grundlage ist das Feld „is_day“ von Open-Meteo, also Sonnenauf- und -untergang am Ort. |

## Wetterzustände

Die Beschreibung und das Symbol ergeben sich aus dem WMO-Wettercode, den Open-Meteo liefert.

| Code | Beschreibung | Symbol |
|---|---|---|
| 0 | Klar | Sonne / Mond |
| 1 | Überwiegend klar | Sonne/Mond mit kleiner Wolke |
| 2 | Teilweise bewölkt | Sonne/Mond mit Wolke |
| 3 | Bedeckt | zwei Wolken |
| 45, 48 | Nebel, Nebel mit Reif | Wolke mit Nebelstreifen |
| 51, 53, 55 | Leichter / – / Starker Nieselregen | Wolke mit kurzen Tropfen |
| 56, 57 | Gefrierender Niesel | Wolke mit kurzen Tropfen |
| 61, 63, 65 | Leichter Regen / Regen / Starker Regen | Wolke mit Regen |
| 66, 67 | Gefrierender Regen | Wolke mit Regen |
| 71, 73, 75 | Leichter / – / Starker Schneefall | Wolke mit Flocken |
| 77 | Schneegriesel | Wolke mit Flocken |
| 80, 81, 82 | Leichte / – / Heftige Regenschauer | Sonne/Mond, Wolke, Regen |
| 85, 86 | Schneeschauer | Sonne/Mond, Wolke, Flocken |
| 95 | Gewitter | dunkle Wolke mit Blitz |
| 96, 99 | Gewitter mit Hagel | dunkle Wolke mit Blitz |

## Zeit und Ort

- Die Uhrzeit kommt per NTP aus dem Internet. Als Zeitzone gilt der UTC-Versatz, den Open-Meteo für den Ort liefert. Die Umstellung auf Sommer- oder Winterzeit wird damit spätestens beim nächsten Abruf übernommen, also nach höchstens 10 Minuten.
- Wird der Ort über die IP-Adresse bestimmt, kann er einige Kilometer danebenliegen. Bestimmt wird er einmal nach dem Start.

## Bekannte Grenzen

- Kein Einstellungsmenü auf dem Gerät. WLAN und Ort werden im Quellcode eingetragen.
- Nur ein Ort.
- Texte nur auf Deutsch. Die Schriften enthalten ASCII, deutsche Umlaute, ß, °, ·, – und …. Andere Sonderzeichen, etwa in Ortsnamen, werden nicht dargestellt.
