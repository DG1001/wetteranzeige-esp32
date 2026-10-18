# Technische Details

## Hardware

Sunton **ESP32-8048S043** (Platinenaufdruck „…F1 V1.2“, Variante mit kapazitivem Touch)

| Baugruppe | Details |
|---|---|
| Modul | ESP32-S3-WROOM-1 **N16R8**: 16 MB Flash (QIO), 8 MB Octal-PSRAM |
| Display | 4,3", 800×480, 16-Bit-RGB-Schnittstelle (RGB565) |
| Touch | Goodix GT911 über I²C, auf **480×272** eingestellt (nicht 800×480) |
| USB | USB-C mit Seriell-Wandler CH340, **keine CC-Widerstände** (siehe [README](README.md)) |
| Sonstiges | microSD-Steckplatz, Tasten BOOT und RST |

### Pinbelegung

| Funktion | GPIO |
|---|---|
| R0–R4 | 45, 48, 47, 21, 14 |
| G0–G5 | 5, 6, 7, 15, 16, 4 |
| B0–B4 | 8, 3, 46, 9, 1 |
| HSYNC / VSYNC / DE / PCLK | 39 / 41 / 40 / 42 |
| Hintergrundbeleuchtung (PWM) | 2 |
| Touch SDA / SCL / RST | 19 / 20 / 38 (INT nicht genutzt) |

### Display-Timing (`src/lcd.cpp`)

| Parameter | Wert |
|---|---|
| Pixeltakt | 16 MHz, Daten auf fallender Flanke (`pclk_active_neg`) |
| HSYNC: Pulsbreite / Back Porch / Front Porch | 4 / 16 / 8 |
| VSYNC: Pulsbreite / Back Porch / Front Porch | 4 / 4 / 4 |

Ergibt etwa 39 Bilder pro Sekunde. **Bei 12 MHz erkennt das Panel kein gültiges Signal** und zeigt seinen Selbsttest (rot, grün, blau, schwarz im Wechsel), deshalb nicht unter 14 MHz gehen.

## Software-Plattform

- **pioarduino** `55.03.312-1`: Arduino-Core 3.3.12 auf ESP-IDF 5.5.5. Die offizielle PlatformIO-Plattform unterstützt Arduino 3.x nicht.
- Board-Definition `esp32-s3-devkitc-1` mit `memory_type = qio_opi`, Partitionstabelle `default_16MB.csv`. Die Datei muss im Projektordner liegen, weil der Hybrid-Build sie sonst nicht findet.
- Bibliotheken: LovyanGFX (nur zum Zeichnen, nicht als Display-Treiber) und ArduinoJson 7

### Eigene ESP-IDF-Einstellungen (`custom_sdkconfig`)

| Option | Zweck |
|---|---|
| `CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y` | Laut Espressif nötig für den Bounce Buffer, sonst kann das Bild wandern |
| `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` | Code und Konstanten laufen aus dem PSRAM. Flash-Schreibzugriffe (WLAN, NVS) sperren dann den Cache nicht mehr. |
| `CONFIG_LCD_RGB_ISR_IRAM_SAFE=y` | Der Display-Interrupt läuft auch während Flash-Zugriffen weiter |

`custom_sdkconfig` löst einen **Hybrid-Build** aus: pioarduino lädt ESP-IDF und baut die Arduino-Bibliotheken neu. Das dauert beim ersten Mal 10 bis 20 Minuten und **ändert das gemeinsame Paket** `~/.platformio/packages/framework-arduinoespressif32-libs`, was andere Arduino-3-Projekte auf demselben Rechner betrifft. Erzeugte Dateien (`sdkconfig.*`, `managed_components/`, `.dummy/`) stehen in `.gitignore`.

## Display-Ansteuerung (`src/lcd.cpp`)

Treiber ist `esp_lcd_new_rgb_panel` aus ESP-IDF:

- **Bounce Buffer** (`bounce_buffer_size_px = 800 × 10`): Zwei Puffer für je 10 Zeilen im internen RAM. Die CPU kopiert per Interrupt aus dem PSRAM nach, die DMA des Displays liest nur aus dem internen RAM. Kurze Engpässe beim PSRAM-Zugriff (WLAN, große Kopien) führen dadurch nicht mehr zu einem springenden Bild.
- **Zwei Bildspeicher** im PSRAM (`num_fbs = 2`, je 768 KB):
  1. LovyanGFX zeichnet über ein `LGFX_Sprite` mit fremdem Puffer (`setBuffer`) in den unsichtbaren Speicher.
  2. `lcd::show()` ruft `esp_lcd_panel_draw_bitmap` mit diesem Speicher auf. Der Treiber erkennt seinen eigenen Bildspeicher, kopiert nichts und schaltet beim nächsten Bildwechsel um.
  3. Danach wird auf den Callback `on_frame_buf_complete` gewartet (Semaphore, höchstens 100 ms). Erst dann ist der andere Speicher frei zum Zeichnen.
  - Jedes Bild wird komplett neu gezeichnet, weil der Inhalt des freien Speichers zwei Bilder alt ist.
- **Byte-Reihenfolge:** LovyanGFX speichert RGB565 mit vertauschten Bytes: erstes Byte `RRRRRGGG`, zweites Byte `GGGBBBBB`. Der RGB-Treiber erwartet die umgekehrte Reihenfolge. Statt jedes Pixel umzudrehen, sind die Datenleitungen in `DATA_PINS` passend angeordnet: G3 G4 G5 R0–R4 B0–B4 G0 G1 G2.
- **Hintergrundbeleuchtung:** LEDC-PWM mit 20 kHz und 8 Bit, über `lcd::setBrightness()`.

## Touch (`src/touch.cpp`)

- Reset über GPIO 38, danach wird die Adresse gesucht: erst 0x5D, dann 0x14. Gefunden wurde 0x5D. Welche Adresse gilt, hängt vom Pegel am INT-Pin während des Resets ab.
- Die eingestellte Auflösung wird aus den Registern `0x8048`–`0x804B` gelesen (480×272) und auf 800×480 hochgerechnet.
- Abfrage: Statusregister `0x814E` (Bit 7 = neue Daten, Bits 0–3 = Anzahl der Punkte), erster Punkt ab `0x8150`, danach Status auf 0 zurücksetzen.

## Zeichnen und Schriften

- Zum Zeichnen wird LovyanGFX benutzt: Kanten geglättet, mit Funktionen wie `fillSmoothCircle`, `drawWideLine`, `fillSmoothRoundRect` und `fillGradientRect`.
- **Schriften:** LovyanGFX bringt keine gut aussehende Schrift mit Umlauten mit. Deshalb erzeugt `tools/make_vlw.py` kantengeglättete VLW-Schriften (Format von TFT_eSPI/LovyanGFX) aus DejaVu Sans und legt sie als C-Arrays ab:

  | Name | Schrift | Größe | Zeichen | Größe im Flash |
  |---|---|---|---|---|
  | `font_small` | DejaVu Sans | 18 px | Text | 17 KB |
  | `font_text` | DejaVu Sans | 24 px | Text | 28 KB |
  | `font_bold` | DejaVu Sans Bold | 30 px | Text | 46 KB |
  | `font_clock` | DejaVu Sans Bold | 52 px | Ziffern | 20 KB |
  | `font_huge` | DejaVu Sans Bold | 112 px | Ziffern | 91 KB |

  „Text“ heißt ASCII plus ÄÖÜäöüß°·–…, „Ziffern“ heißt ` -+0123456789°:.%`.
  VLW-Format: Kopf aus 6 × int32 (Big Endian), dann je Zeichen 7 × int32 (Unicode, Höhe, Breite, Vorschub, Abstand Oberkante–Grundlinie, x-Versatz, Füllfeld), danach 8-Bit-Alpha-Bitmaps. Die Zeichen müssen nach Unicode sortiert sein, weil LovyanGFX binär sucht.
- `loadFont()` legt die Zeichentabelle bei jedem Aufruf neu an. `ui.cpp` lädt deshalb nur, wenn sich die Schrift ändert (`currentFont`).
- LovyanGFX rendert VLW-Zeichen per `alloca` auf dem Stack. Für die 112-px-Ziffern reicht der Standard-Stack von 8 KB nicht, deshalb `SET_LOOP_TASK_STACK_SIZE(32 * 1024)`.
- Die Wettersymbole werden in `icons.cpp` aus Kreisen, abgerundeten Rechtecken, breiten Linien und Dreiecken zusammengesetzt und skalieren mit der Größe. Für die Mondsichel wird ein zweiter Kreis in Hintergrundfarbe darübergelegt. Dafür berechnet `backgroundAt()` die Farbe des Verlaufs an der jeweiligen Stelle.

## Datenquellen (`src/weather.cpp`)

### Open-Meteo (Wetter)
`https://api.open-meteo.com/v1/forecast`, kein Schlüssel, per HTTPS. Das Zertifikat wird nicht geprüft (`setInsecure`).

- `current`: temperature_2m, apparent_temperature, relative_humidity_2m, weather_code, wind_speed_10m, wind_direction_10m, is_day, surface_pressure
- `hourly`: temperature_2m, precipitation_probability, mit `forecast_hours=24` ab der aktuellen Stunde
- `daily`: weather_code, temperature_2m_max/min, precipitation_probability_max, sunrise, sunset, mit `forecast_days=6`
- `timezone=auto`: Zeitangaben in Ortszeit, `utc_offset_seconds` wird für NTP verwendet
- Die Antwort ist wenige KB groß und wird komplett mit ArduinoJson eingelesen.

### ip-api.com (Standort)
Nur wenn `LOCATION_NAME` leer ist: `http://ip-api.com/json/?fields=status,city,lat,lon&lang=de`. Die kostenlose Variante gibt es nur ohne HTTPS.

### Zeit
`configTime(utcOffset, 0, "pool.ntp.org", "time.cloudflare.com")` wird nur aufgerufen, wenn sich der Versatz ändert. Den Zeitpunkt des letzten Abrufs rechnet der Status aus `millis()` zurück, weil die Uhr beim ersten Abruf noch nicht gestellt ist.

## Ablauf (`src/main.cpp`)

1. `setup`: Display, Hintergrundbeleuchtung, Touch, WLAN verbinden (höchstens 20 s, `WiFi.persistent(false)`, automatisches Wiederverbinden)
2. `loop` alle 20 ms:
   - Touch: sofortiger Abruf, höchstens einmal pro Sekunde, solange der Finger aufliegt
   - Wetterabruf, wenn `nextFetch` erreicht ist (10 min nach Erfolg, 1 min nach Fehler)
   - Neu gezeichnet wird bei neuen Daten und bei jedem Minutenwechsel, immer der ganze Bildschirm.

## Ressourcen (Stand der Umstellung auf Arduino 3.x)

- RAM (statisch): ca. 50 KB von 320 KB
- Flash: ca. 1,37 MB von 6,5 MB (App-Partition)
- PSRAM: 2 × 768 KB Bildspeicher und dynamische Puffer (TLS, JSON)

## Upload

- `upload_speed = 460800`. Bei 921600 bricht esptool mit „Unable to verify flash chip connection“ ab.
- USB-Seriell über CH340, am Mac `/dev/cu.usbserial-*`
