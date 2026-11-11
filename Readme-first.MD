# WLED Cardputer: Einstieg

Dieses Dokument erklärt, was die Firmware auf dem M5Stack Cardputer macht, wie sie aufgebaut ist und wie sie gebaut und geflasht wird. Die Firmware basiert auf WLED und ergänzt dessen LED-Steuerung um das eingebaute Display und die Tastatur des Cardputers.

## Was das Programm macht

WLED läuft auf dem ESP32-S3 des Cardputers und steuert einen angeschlossenen LED-Streifen. Die WLED-Weboberfläche und die WLED-Funktionen wie Effekte, Farben, Segmente, Presets, WLAN und OTA-Updates bleiben erhalten. Zusätzlich zeigt ein LVGL-Bildschirm Statusinformationen an und die Tastatur ermöglicht die direkte Bedienung.

Der Display-Usermod startet das Cardputer-Display und die Tastatur, lädt die LVGL-Oberfläche und liest gespeicherte Tastenbelegungen aus dem nichtflüchtigen ESP32-Speicher. In der Hauptschleife werden Tastatureingaben verarbeitet, LVGL bedient und die Statusanzeige alle 500 ms aktualisiert.

## Bedienung

| Eingabe | Funktion |
| --- | --- |
| G0 (GPIO 0) | LEDs ein- oder ausschalten |
| `f` | LEDs mit WLEDs konfigurierter Übergangszeit ein- oder ausblenden |
| `,` / `/` | Helligkeit um 25 verringern / erhöhen |
| `;` / `.` | Nächsten / vorherigen Effekt auswählen |
| `h` | Hilfe anzeigen oder zur Hauptanzeige zurückkehren |
| `c` | Zuordnung der Zifferntasten konfigurieren |
| `1` bis `5` | Den für die jeweilige Taste gespeicherten Effekt starten |

### Effekte den Tasten 1 bis 5 zuweisen

1. `c` drücken, um den Zuordnungsbildschirm zu öffnen.
2. Mit `.` und `;` durch die aktuelle WLED-Effektliste blättern.
3. `1`, `2`, `3`, `4` oder `5` drücken, um den angezeigten Effekt der entsprechenden Taste zuzuweisen.
4. `c` drücken, um zur Hauptanzeige zurückzukehren.

Die Zuordnungen werden mit der ESP32-Preferences/NVS-Funktion gespeichert und bleiben nach einem Neustart erhalten. Nicht belegte Zifferntasten starten keinen Effekt. Außerhalb des Zuordnungsbildschirms schalten sie den gespeicherten Effekt des Hauptsegments ein.

## Firmware-Bausteine

### WLED-Kern: `wled00/`

Dieser Ordner enthält die WLED-Firmware: LED-Ausgabe und Busverwaltung, Effekte und Paletten, Helligkeit und Übergänge, Presets, Netzwerk- und Webserverfunktionen, JSON-/WebSocket-Schnittstellen sowie OTA-Updates. Der Cardputer-Build verwendet diesen Kern unverändert als Basis und ergänzt ihn durch einen Usermod.

### Cardputer-Usermod: `usermods/Cardputer_Display/`

`cardputer_display.cpp` bindet das M5Stack-Display und die Tastatur an WLED an. Dort liegen die Tastenbehandlung, das Ein-/Ausblenden, die Effekt-Zuordnung für `1` bis `5`, deren dauerhafte Speicherung und die periodische Aktualisierung der Statusanzeige.

`library.json` beschreibt den Usermod und seine Bibliotheksabhängigkeiten. `lv_conf.h` konfiguriert LVGL. `lvgl_setup.h` erstellt ein 240 × 135 Pixel großes LVGL-Display, verbindet den Bildpuffer mit M5GFX und stellt den LVGL-Tick über eine FreeRTOS-Aufgabe bereit.

### Display-Oberfläche: `usermods/Cardputer_Display/ui/`

Die C-Dateien in `ui/` enthalten die aus dem EEZ Studio-Projekt erzeugten LVGL-Bildschirme, Widgets, Stile, Schriften und Bilder. Das zugehörige Designprojekt liegt in `wled-cardputer-eez/`. Die generierten Dateien sollten nicht von Hand bearbeitet werden; Änderungen am gestalteten Hauptbildschirm gehören ins EEZ-Projekt. Hilfe- und Effekt-Zuordnungsbildschirm werden dagegen direkt vom Usermod erzeugt.

### Board- und Build-Konfiguration

- `platformio.ini` definiert die Umgebung `m5stack_cardputer`, das Board ESP32-S3, die Bibliotheken, Build-Flags, den Display-Usermod und die Partitionstabelle.
- `tools/WLED_ESP32_8MB.csv` legt das Partitionslayout für den 8-MB-Flash fest. Das konfigurierte Board hat keinen PSRAM.
- `lib/NeoESP32RmtHI/` enthält den projektspezifischen ESP32-RMT-LED-Treiber.
- `pio-scripts/` enthält PlatformIO-Hooks für Metadaten, lokale WLED-Konfiguration, Usermod-Einbindung, Web-UI-Build, Modulprüfung und Ausgabe der Firmware-Dateien.
- `wled00/data/` enthält die WLED-Weboberfläche. Beim Build bündelt `tools/cdata.js` deren Dateien zu Firmware-Headern.
- `requirements.txt` pinnt die Python-Buildwerkzeuge, einschließlich PlatformIO. `package.json` und `package-lock.json` definieren die Node.js-Werkzeuge für die Web-UI.

### Start und Statusanzeige

Beim Start initialisiert WLED die Board- und LED-Konfiguration. Der Cardputer-Usermod aktiviert anschließend Display und Tastatur, initialisiert LVGL, lädt den Hauptbildschirm und stellt die Effekt-Tastenzuordnungen aus dem Gerätespeicher wieder her.

Der Hauptbildschirm zeigt WLAN-Status und IP-Adresse, LED-Pin und -Anzahl, Helligkeit, aktuellen Effekt, Uhrzeit und Datum, geschätzten Stromverbrauch, den vom Cardputer gemeldeten Akku-Ladestand sowie freien Heap-Speicher. Der Akku-Prozentwert ist eine Schätzung aus der gemessenen Akkuspannung. WLAN und weitere Grundeinstellungen können wie bei WLED über dessen WLAN-Zugangspunkt und Weboberfläche eingerichtet werden.

## Bauen unter Windows

Voraussetzungen: Git, Python und Node.js 20 oder neuer. PlatformIO ist in `requirements.txt` festgelegt. Eine virtuelle Umgebung außerhalb des Repositories vermeidet zusätzliche Build-Dateien im Quellbaum:

```powershell
$env:BUILD_VENV = "$env:LOCALAPPDATA\WLED-Cardputer-venv"
py -m venv $env:BUILD_VENV
& "$env:BUILD_VENV\Scripts\python.exe" -m pip install -r requirements.txt
& "$env:BUILD_VENV\Scripts\pio.exe" run -e m5stack_cardputer
```

Der erste Build lädt Plattform, Compiler und Bibliotheken herunter. Der PlatformIO-Hook installiert außerdem die Node.js-Pakete und bündelt die WLED-Weboberfläche.

## Flashen

Cardputer mit einem USB-Datenkabel anschließen und den COM-Port im Windows-Geräte-Manager ermitteln. `COMx` im folgenden Befehl durch den Cardputer-Port ersetzen:

```powershell
& "$env:LOCALAPPDATA\WLED-Cardputer-venv\Scripts\pio.exe" run -e m5stack_cardputer -t upload --upload-port COMx
```

Das rohe PlatformIO-Image liegt unter `.pio/build/m5stack_cardputer/firmware.bin`. Der Build-Hook kopiert es zusätzlich nach `build_output/release/WLED_0.16.0-alpha_Cardputer.bin`. Für den normalen Flash-Vorgang ist PlatformIOs Upload-Target zu bevorzugen, da es das konfigurierte Board- und Partitionslayout verwendet.

## Konfiguration und Grenzen

`wled00/my_config.h` wird beim ersten Build automatisch aus `wled00/my_config_sample.h` erzeugt und ist für lokale WLED-Compile-Optionen vorgesehen. WLAN-Zugangsdaten sollten nicht in gemeinsam genutzte Quellstände eingecheckt werden.

Der Standard-LED-Ausgang ist in `platformio.ini` auf GPIO 21 gesetzt. Vor dem Anschließen des LED-Streifens müssen dessen Versorgung und Masse korrekt ausgelegt und mit der Cardputer-Masse verbunden sein. Der GPIO ist ein Datenausgang und versorgt den LED-Streifen nicht mit Strom.

## Lizenz

Das Projekt basiert auf WLED und verwendet die in `LICENSE` angegebene EUPL-1.2-Lizenz. Für WLED und eingebundene Bibliotheken gelten zusätzlich deren jeweilige Lizenzbedingungen.