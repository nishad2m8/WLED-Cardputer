# WLED Cardputer: Getting Started

This document explains what the firmware does on the M5Stack Cardputer, how it is structured, and how to build and flash it. The firmware is based on WLED and extends its LED control with the Cardputer's built-in display and keyboard.

## What the Firmware Does

WLED runs on the Cardputer's ESP32-S3 and controls a connected LED strip. The WLED web interface and features such as effects, colors, segments, presets, WiFi, and OTA updates remain available. An LVGL screen also displays status information, while the keyboard provides direct local control.

The display usermod initializes the Cardputer display and keyboard, loads the LVGL interface, and handles local input. Its main loop processes keyboard input, services LVGL, and refreshes the status display every 500 ms. Keyboard shortcuts and the WiFi enabled/disabled state are saved in WLED configuration and restored at startup.

## Controls

| Input | Action |
| --- | --- |
| G0 (GPIO 0) | Toggle LEDs on or off |
| `f` | Fade LEDs using WLED's configured transition time |
| `,` / `/` | Decrease / increase brightness by 25 |
| `;` / `.` | Select the next / previous effect |
| `h` | Show help or return to the main display |
| `w` | Toggle the Cardputer's WiFi radio |
| `c` | Open the configuration menu or go back |
| `1` to `5` | Start the assigned effect; assign it in the effect-shortcut menu |

The `f` key uses the transition time configured in WLED. When WiFi is off, the web interface, network control, and OTA are unavailable. The WiFi state is saved in WLED configuration and restored after a restart.

### Configuration Menu and Effect Shortcuts

Press `c` to open the configuration menu. Use `;` and `.` to move through the options, and Enter to select or edit an item. Press `c` to cancel an edit or go back. In numeric fields, `,` and `/` decrease or increase the value; digits can also be typed directly and removed individually with Del.

In **Effect key shortcuts**, browse the WLED effect list with `.` and `;`. Press `1` through `5` to assign the selected effect to the corresponding shortcut. To clear a shortcut, press `0` and then its number; press `c` to cancel deletion. Unassigned keys do not start an effect. Assignments are saved in WLED configuration. Older assignments stored in ESP32 Preferences are imported at startup when valid assignments are not present in WLED configuration.

The menu also configures the LED type, output pin, LED count, color order, direction, skipped LEDs, current per LED, per-output and global current limits, and RGBW white-channel mode where supported by the LED type. The LED options edit the first LED bus; configure additional buses through the normal WLED configuration interface. Bus changes are saved through WLED configuration and restart the LED output.

## Firmware Components

### WLED Core: `wled00/`

This directory contains the WLED firmware, including LED output and bus management, effects and palettes, brightness and transitions, presets, networking and web server functionality, JSON and WebSocket interfaces, and OTA updates. The Cardputer build is based on this core and extends it with a usermod.

### Cardputer Usermod: `usermods/Cardputer_Display/`

`cardputer_display.cpp` connects the display and keyboard to WLED. It handles keyboard input, the configuration and shortcut menus, LED fades, the WiFi toggle, status display, and reading and writing Cardputer settings in WLED configuration.

`library.json` describes the usermod and its library dependencies. `lv_conf.h` configures LVGL. `lvgl_setup.h` creates a 240 × 135 pixel LVGL display, connects its drawing buffer to M5GFX, and provides LVGL ticks through a FreeRTOS task.

### Display Interface: `usermods/Cardputer_Display/ui/`

The C files in `ui/` contain the LVGL screens, widgets, styles, fonts, and images generated from the EEZ Studio project. The design project is in `wled-cardputer-eez/`. Do not edit generated files by hand; make changes to the designed main screen in the EEZ project. The help and effect-shortcut screens are created directly by the usermod.

### Board and Build Configuration

- `platformio.ini` defines the `m5stack_cardputer` environment, ESP32-S3 board, libraries, build flags, display usermod, and partition table.
- `tools/WLED_ESP32_8MB.csv` defines the partition layout for 8 MB of flash. The configured board has no PSRAM.
- `lib/NeoESP32RmtHI/` contains the project-specific ESP32 RMT LED driver.
- `pio-scripts/` contains PlatformIO hooks for metadata, local WLED configuration, usermod integration, web UI building, module validation, and firmware output.
- `wled00/data/` contains the WLED web interface. During the build, `tools/cdata.js` bundles its files into firmware headers.
- `requirements.txt` pins the Python build tools, including PlatformIO. `package.json` and `package-lock.json` define the Node.js tools used for the web interface.

### Startup and Status Display

At startup, WLED initializes the board and LED configuration. The Cardputer usermod then initializes the display, keyboard, and LVGL, loads the main screen, and restores the WiFi state and effect shortcuts from WLED configuration. If effect shortcuts are missing, the usermod can import older values from ESP32 Preferences and mark the WLED configuration for saving.

The main screen shows WiFi status and IP address, LED pin and count, brightness, current effect, time and date, estimated current draw, the battery level reported by the Cardputer, and free heap memory. The battery percentage is an estimate based on measured battery voltage. Configure WiFi credentials and other basic settings through WLED's access point and web interface; the `w` key can then toggle the WiFi radio locally.

## Building on Windows

Requirements: Git, Python, and Node.js 20 or newer. PlatformIO is pinned in `requirements.txt`. To avoid adding build files to the source tree, create the virtual environment outside the repository:

```powershell
$env:BUILD_VENV = "$env:LOCALAPPDATA\WLED-Cardputer-venv"
py -m venv $env:BUILD_VENV
& "$env:BUILD_VENV\Scripts\python.exe" -m pip install -r requirements.txt
& "$env:BUILD_VENV\Scripts\pio.exe" run -e m5stack_cardputer
```

The first build downloads the platform, compiler, and libraries. A PlatformIO hook also installs the Node.js packages and bundles the WLED web interface.

## Flashing

Connect the Cardputer with a USB data cable and find its COM port in Windows Device Manager. Replace `COMx` in the following command with the Cardputer's port:

```powershell
& "$env:LOCALAPPDATA\WLED-Cardputer-venv\Scripts\pio.exe" run -e m5stack_cardputer -t upload --upload-port COMx
```

The raw PlatformIO image is at `.pio/build/m5stack_cardputer/firmware.bin`. The build hook also copies it to `build_output/release/WLED_0.16.0-alpha_Cardputer.bin`. For normal flashing, prefer PlatformIO's upload target because it uses the configured board and partition layout.

## Configuration and Limitations

On the first build, `wled00/my_config.h` is created from `wled00/my_config_sample.h` and is intended for local WLED compile-time options. Do not commit WiFi credentials to shared source branches.

The default LED output is GPIO 21, as defined in `platformio.ini`. The configuration menu edits the first LED bus and only offers suitable, available GPIO pins. Before connecting an LED strip, ensure its power supply and ground are properly sized and connect its ground to the Cardputer ground. The GPIO is a data output and does not power the LED strip.

## License

This project is based on WLED and uses the EUPL 1.2 license specified in `LICENSE`. WLED and included libraries may also be subject to their own license terms.