# WLED Cardputer

![alt text](elements/preview.gif)

---
<p align="center">
  <span style="color: yellow;">If you like this, consider supporting it:</span>
</p>

<p align="center">
  <a href="https://www.buymeacoffee.com/nishad2m8" target="_blank">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" style="height: 35px;">
  </a>
  <a href="https://www.youtube.com/channel/UCV_35rUyf4N5mHZXaxaFKiQ" target="_blank">
    <img src="https://img.shields.io/badge/Subscribe%20on%20YouTube-FF0000?style=flat&logo=youtube" alt="Subscribe on YouTube" style="height: 35px;">
  </a>
</p>


---

A custom [WLED](https://github.com/wled-dev/WLED) build for the **M5Stack Cardputer**, featuring a built-in LVGL display interface and keyboard control for LED strips.

Built on top of [WLED](https://kno.wled.ge) by [Aircoookie](https://github.com/Aircoookie) — a fast, feature-rich ESP32 webserver for controlling NeoPixel (WS2812B, WS2811, SK6812) and SPI-based (WS2801, APA102) LEDs.

## Features

- Full WLED functionality (100+ effects, 50+ palettes, segments, presets, OTA updates)
- LVGL-based display showing WiFi status, IP, strip brightness, current effect, time, battery level, and system info
- Keyboard controls for hands-on operation without needing the web UI
- Default LED output on GPIO 21, configurable from the C menu
- Five persistent effect shortcuts that can be assigned and cleared individually

## Controls

### Button

| Button | Action |
|--------|--------|
| G0 (GPIO 0) | Toggle LEDs on/off |

### Keyboard

| Key | Action |
|-----|--------|
| `,` | Decrease LED strip brightness |
| `/` | Increase LED strip brightness |
| `;` | Next effect |
| `.` | Previous effect |
| `f` | Fade LEDs on/off |
| `w` | Toggle WiFi |
| `h` | Show or hide keyboard help |
| `c` | Open the configuration menu |
| `1`–`5` | Start the assigned effect |
| `0`, then `1`–`5` | In effect-key setup, clear that shortcut slot |

LED strip brightness changes in steps of 25 (0–255 scale). The display backlight stays at a fixed brightness; there is no keyboard brightness control or inactivity screensaver.
The `f` key uses WLED's configured transition time.
In the C menu, choose **Effect key shortcuts**, browse effects with `.` and `;`, then press `1`–`5` to assign the selected effect. To clear a specific slot, press `0` followed by its number; press `c` to cancel deletion. Shortcut assignments are stored in WLED's configuration and survive restarts.
The C menu also configures the supported single-data-pin LED type, output pin, LED count, color order, direction, skipped LEDs, per-LED current, per-output and global current limits, and RGBW white-channel mode where applicable. Use `;` and `.` to navigate, Enter to edit or save, `,` and `/` to change values, and `c` to cancel or go back. Numeric values can be typed directly; Del erases a digit. Bus changes are saved in WLED configuration and restart the LED output.

## Display Info

The Cardputer's built-in screen shows:

- WiFi SSID and IP address
- LED pin and count
- LED strip brightness (value and bar)
- Battery charge estimate
- Current effect name
- Time and date
- Estimated current draw
- Free heap memory

The display backlight is fixed at startup and does not dim automatically.

## Building

Requires [PlatformIO](https://platformio.org/).

```
pio run -e m5stack_cardputer
```

## Flashing

### PlatformIO

```
pio run -e m5stack_cardputer -t upload
```

### Pre-built Binary

A pre-built `WLED-CardPuter.bin` is available for direct flashing. You can also find it in **M5Burner** and the **M5SLauncher** for easy installation without needing to compile from source.

## License

Licensed under the [EUPL v1.2](LICENSE).

Based on [WLED](https://github.com/wled-dev/WLED) — all credit to the WLED project and its [contributors](https://kno.wled.ge/about/contributors/).
