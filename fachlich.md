# Fachliche Beschreibung

## Zweck

Eine Wetteranzeige für einen festen Ort, die ohne Bedienung läuft. Auf einen Blick zeigt sie, wie das Wetter jetzt ist und wie es wird. Per Wischen und Antippen gibt es Details: die beste Zeit für draußen mit Hinweisen, Regenradar, Stundenverlauf, Tagesansichten, Sonne und Mond, Luftqualität, Wasserbilanz für den Garten, Pollenflug und Biowetter sowie amtliche Unwetterwarnungen. Nachts kann sie auf eine gedimmte Uhr umschalten oder ganz dunkel werden. Eingerichtet und aktualisiert wird sie über das WLAN.

## Bedienung

| Geste | Wirkung |
|---|---|
| Nach links / rechts wischen | Nächste / vorige Seite: Übersicht → Draußen → Radar → 48 Stunden → Himmel & Luft → Gesundheit |
| Tageskachel antippen (Übersicht) | Tagesansicht dieses Tages |
| In der Tagesansicht wischen | Nächster / voriger Tag |
| Stundenkarte antippen (Übersicht) | Seite „48 Stunden“ |
| Warnsymbol antippen (Kopfzeile) | Warnungen mit vollständigem Text |
| Sonst auf der Übersicht antippen | Alle Daten sofort neu abrufen |
| Radar antippen | Animation anhalten / fortsetzen |
| Tagesansicht, Warnungen oder Infoseite antippen | Zurück zur Übersicht |
| 2 Sekunden gedrückt halten | Infoseite: Adresse der Webseite, PIN, QR-Code |
| Nachts antippen | Display für 1 Minute wecken |

Nach 3 Minuten ohne Berührung kehrt die Anzeige zur Übersicht zurück. Die Punkte unter der Kopfzeile zeigen, auf welcher der sechs Wischseiten man ist. Gewischt werden kann jederzeit, auch während Daten oder Radarbilder geladen werden.

## Kopfzeile (auf allen Seiten)

- **Ort**, links oben (in `include/config.h` fest eingestellt: Künzelsau)
- **Datum** ausgeschrieben, z. B. „Montag, 19. Oktober 2026“
- **Uhrzeit** groß, rechts oben
- **Status** darunter:
  - „Stand 22:40“: Zeitpunkt des letzten erfolgreichen Wetterabrufs
  - „Aktualisiere …“: nach dem Antippen
  - „Kein WLAN · Stand …“ bzw. „Abruf fehlgeschlagen · Stand …“ (orange), wenn keine aktuellen Daten da sind
- **Warnsymbol** in der Mitte, nur wenn der DWD für den Ort warnt: Farbe der höchsten Warnstufe (gelb, orange, rot, violett), Name der Warnung, „+N“ bei weiteren Warnungen

## Seiten

### Übersicht
- **Wettersymbol, animiert:** Sonnenstrahlen drehen sich, Wolken ziehen, Regen und Schnee fallen, Blitze zucken, nachts funkeln Sterne um den Mond
- **Temperatur**, **Beschreibung** (z. B. „Teilweise bewölkt“)
- **Wechselnde Zeile** darunter, alle 6 Sekunden:
  - Regenhinweis für die nächsten 2 Stunden aus der 15-Minuten-Vorhersage: „Regen ab ca. 14:30“ bzw. „Regen endet gegen 15:15“ (blau, mit Tropfen), „Regen hält an“ oder „Nächste 2 Stunden trocken“
  - beste Zeit draußen, z. B. „Draußen ideal: 11–19 Uhr“ bzw. „Morgen ideal: …“
  - wichtige Hinweise von der Seite „Draußen“ (ab mittlerer Wichtigkeit, höchstens zwei; sehr wichtige in Orange)
  - zu lange Texte werden mit „…“ gekürzt
- **Details:** gefühlte Temperatur, Luftfeuchte, Wind mit Richtung, Luftdruck (am Standort), Sonnenauf- und -untergang
- **Nächste 24 Stunden:** Temperaturkurve, Regenwahrscheinlichkeit als Balken
- **6 Tage** ab heute: Symbol, Höchst- und Tiefstwert, Regenwahrscheinlichkeit

### Draußen
**Beste Zeit draußen:** Jede Stunde mit Tageslicht bekommt eine Bewertung von 0 bis 100:
- Abzüge für Regenwahrscheinlichkeit und Regen, Kälte unter 15° (höchstens 35 Punkte), Hitze über 26°, Wind über 20 km/h, Böen über 45 km/h, Gewitter, Schnee, Nebel und geschlossene Bewölkung
- ein kleiner Bonus für Sonne
- Stunden in der Nacht oder in der Vergangenheit zählen nicht

Genannt wird der zusammenhängende Zeitraum mit den besten Stunden des Tages: mindestens 50 Punkte und höchstens 12 Punkte unter dem Tagesbestwert. Dazu kommt eine Zusammenfassung wie „trocken · 12–16° · leichter Wind“. Bleiben heute weniger als 2 Stunden Tageslicht, gilt das für morgen. Eine Stundenleiste zeigt den ganzen Tag: grün = gut, gelb = mäßig, rot = schlecht, dunkel = Nacht, grau = vorbei. Der empfohlene Zeitraum ist eingerahmt, ein Pfeil markiert jetzt.

**Hinweise** für die nächsten 24 Stunden, nach Wichtigkeit (höchstens 4 sichtbar):

| Hinweis | Bedingung | Wichtigkeit |
|---|---|---|
| Gewitter möglich um … | Gewitter-Wettercode | 9 |
| Glättegefahr um … | Regen bei höchstens 1° | 9 |
| Sturmböen bis … km/h | Böen ab 65 km/h | 8 |
| Starkregen um … | ab 5 mm in einer Stunde | 8 |
| Schnee um … | Schnee-Wettercode | 7 |
| Frost bis …°: Pflanzen schützen | nachts höchstens 0° | 7 |
| Schirm mitnehmen: Regen um … | tagsüber Regenwahrscheinlichkeit ab 60 % | 6 |
| Heiß bis …°: viel trinken | Höchstwert ab 30° | 6 |
| Bodenfrost möglich | nachts höchstens 3° | 5 |
| UV-Index bis …: Sonnenschutz | UV-Maximum ab 6 | 5 |
| Windig, Böen bis … km/h | Böen 50 bis 65 km/h | 4 |
| Nebel um … | Nebel in den nächsten 12 Stunden | 4 |
| Pollenflug heute: … hoch / mittel bis hoch / mittel | DWD-Pollenflug heute ab Stufe 2 (stärkste Art) | 6 bzw. 5 |
| Biowetter: hohe Belastung (…) | DWD-Biowetter heute „hohe Gefährdung“ | 4 |
| Gießen: in 7 Tagen … mm zu wenig Regen | Wasserbilanz ≤ −10 mm und heute und morgen zusammen unter 5 mm Regen erwartet | 4 |
| Gutes Wetter zum Wäschetrocknen | mindestens 5 Stunden trocken, ab 8°, Luftfeuchte unter 75 % oder Wind ab 10 km/h | 3 |

### Regenradar
- Karte von etwa 300 km × 150 km rund um den Ort: abgedunkeltes Relief, Flüsse, Landesgrenzen, Städte. Der eigene Ort ist hervorgehoben.
- Niederschlag vom DWD von **1 Stunde vor bis 2 Stunden nach jetzt** in 10-Minuten-Schritten, als Endlosschleife
- Zeitleiste: Vergangenheit grau („Messung“), letzte Messung weiß („Aktuell“), Vorhersage blau („Vorhersage“)
- Die Bilder werden beim Öffnen der Seite geladen, das dauert einige Sekunden (Fortschritt in Prozent). Danach werden sie alle 10 Minuten erneuert.

### 48 Stunden
Stündlicher Verlauf ab jetzt:
- Wettersymbol alle 3 Stunden (nachts mit Mond)
- Temperaturkurve
- Regen: helle Balken für die Wahrscheinlichkeit, kräftige für die Menge (4 mm/h entsprechen voller Höhe)
- Wind und Böen in km/h, Uhrzeit
- Tageswechsel als Linie mit Wochentag und Datum

### Tagesansicht
- Wochentag und Datum, Symbol, Beschreibung, Höchst- und Tiefstwert
- Regenmenge und -wahrscheinlichkeit, stärkste Böe, höchster UV-Index, Sonnenauf- und -untergang
- Stundenverlauf des Tages wie auf der Seite „48 Stunden“

### Himmel & Luft
- **Sonne:** Bogen von Aufgang bis Untergang mit dem aktuellen Sonnenstand, Tageslänge und Unterschied zu gestern. Nachts statt der Sonne „Aufgang morgen …“.
- **Mond:** Phase als Bild, Name (z. B. „Erstes Viertel“), beleuchteter Anteil, nächster Voll- und Neumond. Berechnet wird mit dem mittleren Mondlauf, Abweichung bis etwa einen halben Tag.
- **UV-Index:** aktueller Wert mit Einstufung und Farbskala, Höchstwert heute
  - 0–2 niedrig, 3–5 mäßig, 6–7 hoch, 8–10 sehr hoch, ab 11 extrem
- **Luftqualität:** Europäischer Luftqualitätsindex mit Einstufung, dazu Feinstaub PM2,5 und PM10, Ozon und Stickstoffdioxid in µg/m³
  - 0–20 gut, 20–40 akzeptabel, 40–60 mäßig, 60–80 schlecht, 80–100 sehr schlecht, darüber extrem schlecht
- **Garten:** Wasserbilanz der letzten 7 Tage, also Regen minus Verdunstung (FAO-Referenzverdunstung von Open-Meteo), mit Balken je Tag (Regen blau nach oben, Verdunstung orange nach unten). Bewertung:
  - über 0 mm: „ausreichend Regen“
  - −10 bis 0 mm: „etwas trocken“
  - ab −10 mm: „trocken: gießen“ (orange), bzw. „trocken, aber Regen erwartet“, wenn heute und morgen zusammen mindestens 5 mm Regen kommen sollen

### Gesundheit
Beides vom Deutschen Wetterdienst (Medizin-Meteorologie), Region bzw. Zone auf der Webseite einstellbar (für Künzelsau: Pollenregion „Hohenlohe/mittlerer Neckar/Oberschwaben“, Biowetter-Zone „Württemberg, Franken“).
- **Pollenflug:** Hasel, Erle, Esche, Birke, Gräser, Roggen, Beifuß, Ambrosia für heute, morgen und übermorgen, als farbige Felder mit der Stufe des DWD (0, 0–1, 1, 1–2, 2, 2–3, 3; 0 = keine, 1 = gering, 2 = mittel, 3 = hoch). Der DWD gibt die Vorhersage einmal täglich gegen 11 Uhr aus. Vor der neuen Ausgabe fehlt deshalb meist der dritte Tag („–“).
- **Biowetter:** Einfluss des Wetters auf Allgemeinbefinden, niedrigen Blutdruck, Herz und Bluthochdruck, entzündliches und degeneratives Rheuma, Asthma sowie Wärme- bzw. Kältebelastung. Für heute gilt die aktuelle Tageshälfte, für morgen die ungünstigere der beiden. Stufen: – kein Einfluss, günstig (grün), gering (gelb), mittel (orange), hoch (rot); bei Wärme/Kälte steht die Art im Feld, die Farbe zeigt die Stärke. Darunter die Empfehlung des DWD für heute.

### Warnungen
Alle aktuellen amtlichen Warnungen und Vorabinformationen des DWD für die Gemeinde am Standort, nach Stufe sortiert. Jede Warnung zeigt Überschrift, Zeitraum, Beschreibung und Handlungsempfehlung. Lange Texte werden mit „…“ gekürzt.

### Nacht
Im eingestellten Zeitraum (z. B. 22:30 bis 6:30) zeigt die Anzeige eine große, gedimmte Uhr mit Datum, Temperatur und gegebenenfalls einer Warnung, auf schwarzem Grund. Alternativ wird das Display ganz dunkel. Antippen weckt die normale Anzeige für eine Minute. Der Nachtmodus wird auf der Webseite eingeschaltet, ab Werk ist er aus.

### Infoseite
Adresse der Webseite (`http://wetteranzeige.local` und die IP-Adresse), Benutzer `admin`, PIN und ein QR-Code mit der Adresse.

## Einrichtung und Webseite
- **Erster Start oder WLAN nicht erreichbar:** Die Anzeige öffnet einen eigenen Hotspot „Wetteranzeige“. Das Passwort ist die PIN, der Bildschirm zeigt sie zusammen mit einem QR-Code zum Verbinden. Nach dem Verbinden öffnet sich die Einrichtungsseite (sonst `http://192.168.4.1`). Dort WLAN wählen, Passwort und Ort eintragen und speichern, danach startet die Anzeige neu.
  - Ist niemand mit dem Hotspot verbunden, versucht die Anzeige alle 30 Sekunden das gespeicherte WLAN und startet neu, sobald es erreichbar ist. Das hilft etwa, wenn der Router nach einem Stromausfall länger braucht. Nach 5 Minuten ohne Eingabe startet sie ebenfalls neu.
- **Webseite** im Heimnetz unter `http://wetteranzeige.local`, Benutzer `admin`, Passwort = PIN:
  - Ort per Name (wird gesucht, Treffer in Deutschland bevorzugt) oder mit Koordinaten; leer = Bestimmung über die IP-Adresse
  - Helligkeit tagsüber und nachts
  - Nachtmodus: ein/aus, Beginn, Ende, gedimmte Uhr oder Display aus, Helligkeit der Uhr
  - WLAN ändern (danach Neustart)
  - Region für Pollenflug, Zone für Biowetter
  - neue PIN (8 Ziffern)
  - Bildschirmfoto
  - Statusseite: Laufzeit, Grund des letzten Starts, Speicher, WLAN-Signal, Zustand jeder Datenquelle, Log der letzten 64 Meldungen
  - Firmware-Update (Datei hochladen)
- Die **PIN** wird beim ersten Start zufällig erzeugt. Sie steht nur am Gerät (Infoseite, Einrichtungsbildschirm).

## Himmel als Hintergrund
Der Hintergrund folgt Tageszeit und Wetter:
- Tagsüber blau bei klarem Himmel, grau bei Bewölkung, dunkelgrau bei Regen, violett-grau bei Gewitter, hell graublau bei Schnee
- Nachts dunkelblau, bei Bewölkung grauer
- Morgen- und Abenddämmerung (je 50 Minuten vor und nach Sonnenauf- und -untergang) mit Übergang über ein warmes Orange, bei Bewölkung gedämpft
- Karten auf dem Hintergrund wirken halbtransparent und folgen dem Verlauf.

Die Helligkeit der Beleuchtung ist tagsüber 220 und nachts 60. Beides lässt sich auf der Webseite ändern.

## Aktualisierung

| Daten | Abstand | nach Fehler |
|---|---|---|
| Wetter | 10 min | 1 min |
| Unwetterwarnungen | 5 min | 1 min |
| Luftqualität | 30 min | 1 min |
| Pollenflug und Biowetter (DWD) | 1 h | 1 min |
| Wasserbilanz Garten | 3 h | 1 min |
| Radarbilder | beim Öffnen der Radarseite, wenn älter als 10 min | nächster Durchlauf |
| Radarkarte | einmal nach dem Start | nächster Durchlauf |
| Uhr, Hintergrund, Nachtmodus | jede Minute | – |
| Beste Zeit und Hinweise | mit jedem Wetterabruf | – |

## Wetterzustände

Beschreibung und Symbol ergeben sich aus dem WMO-Wettercode von Open-Meteo.

| Code | Beschreibung | Symbol (Animation) |
|---|---|---|
| 0 | Klar | Sonne (Strahlen drehen sich) / Mond (Sterne funkeln) |
| 1 | Überwiegend klar | Sonne/Mond mit kleiner Wolke |
| 2 | Teilweise bewölkt | Sonne/Mond mit Wolke |
| 3 | Bedeckt | zwei Wolken (ziehen gegeneinander) |
| 45, 48 | Nebel, Nebel mit Reif | Wolke mit Nebelstreifen (wabern) |
| 51, 53, 55 | Leichter / – / Starker Nieselregen | Wolke mit kurzen Tropfen |
| 56, 57 | Gefrierender Niesel | Wolke mit kurzen Tropfen |
| 61, 63, 65 | Leichter Regen / Regen / Starker Regen | Wolke mit Regen |
| 66, 67 | Gefrierender Regen | Wolke mit Regen |
| 71, 73, 75 | Leichter / – / Starker Schneefall | Wolke mit Flocken (schweben) |
| 77 | Schneegriesel | Wolke mit Flocken |
| 80, 81, 82 | Leichte / – / Heftige Regenschauer | Sonne/Mond, Wolke, Regen |
| 85, 86 | Schneeschauer | Sonne/Mond, Wolke, Flocken |
| 95 | Gewitter | dunkle Wolke mit Blitz (Doppelblitz etwa alle 3 s) |
| 96, 99 | Gewitter mit Hagel | dunkle Wolke mit Blitz |

## Bekannte Grenzen

- Einstellungen nur über die Webseite, nicht am Gerät selbst
- Nur ein Ort Warnungen und Radar gibt es nur für Deutschland.
- Texte nur auf Deutsch. Die Schriften enthalten ASCII, Latin-1 und typografische Zeichen wie „“ – … €.
- Radaranimation und Regenhinweis bei tatsächlichem Regen sind bisher nicht am Gerät beobachtet worden, weil es während der Entwicklung trocken war. Dasselbe gilt für die meisten Hinweise auf der Seite „Draußen“; bisher kam nur der Wäschehinweis vor.
- Die Bewertung für „Draußen“ und die Schwellen der Hinweise sind eigene Annahmen, keine offizielle Größe.
- Pollenflug und Biowetter gibt es nur für Deutschland.
