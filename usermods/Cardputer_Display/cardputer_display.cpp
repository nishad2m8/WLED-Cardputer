#include "wled.h"
#include <M5Cardputer.h>
#include <Preferences.h>
#include <lvgl.h>
#include "lvgl_setup.h"
#include "ui/ui.h"
#include "ui/screens.h"

// Brightness step for keyboard control (0-255 scale)
#define BRIGHTNESS_STEP 25

class CardputerDisplayUsermod : public Usermod {
  private:
    static constexpr uint8_t UNASSIGNED_EFFECT = 0xFF;
    bool initDone = false;
    bool helpVisible = false;
    bool shortcutMode = false;
    bool shortcutPreferencesReady = false;
    uint8_t effectShortcuts[5] = {
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT, UNASSIGNED_EFFECT,
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT
    };
    uint8_t selectedEffect = 0;
    Preferences shortcutPreferences;
    lv_obj_t* helpScreen = nullptr;
    lv_obj_t* shortcutScreen = nullptr;
    lv_obj_t* shortcutLabel = nullptr;
    lv_obj_t* batteryLabel = nullptr;
    unsigned long lastRefresh = 0;
    unsigned long lastTickTime = 0;
    static const unsigned long REFRESH_RATE_MS = 500;

    void createHelpScreen() {
      helpScreen = lv_obj_create(nullptr);
      lv_obj_set_style_bg_color(helpScreen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(helpScreen, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_t* helpLabel = lv_label_create(helpScreen);
      lv_label_set_text(helpLabel,
        "KEYBOARD HELP\n"
        "G0  Toggle LEDs\n"
        "f  Fade LEDs on/off\n"
        "c  Configure effect keys\n"
        ",  Brightness -25\n"
        "/  Brightness +25\n"
        ";  Next effect\n"
        ".  Previous effect\n"
        "h  Back to main");
      lv_obj_set_style_text_color(helpLabel, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(helpLabel, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_align(helpLabel, LV_ALIGN_TOP_LEFT, 10, 6);
    }

    void getEffectName(uint8_t effect, char* buffer, size_t bufferSize) {
      if (bufferSize == 0) return;
      strncpy(buffer, strip.getModeData(effect), bufferSize - 1);
      buffer[bufferSize - 1] = '\0';
      char* parameter = strchr(buffer, '@');
      if (parameter) *parameter = '\0';
    }

    void updateShortcutScreen() {
      char selectedName[32];
      char shortcutNames[5][32];
      char screenText[256];
      getEffectName(selectedEffect, selectedName, sizeof(selectedName));

      for (uint8_t index = 0; index < 5; index++) {
        if (effectShortcuts[index] == UNASSIGNED_EFFECT) {
          strcpy(shortcutNames[index], "--");
        } else {
          getEffectName(effectShortcuts[index], shortcutNames[index], sizeof(shortcutNames[index]));
        }
      }

      snprintf(screenText, sizeof(screenText),
        "SET EFFECT KEYS\n"
        "Selected: %s\n"
        ". Previous  ; Next\n"
        "1: %s  2: %s\n"
        "3: %s  4: %s\n"
        "5: %s\n"
        "Press 1-5 to assign\n"
        "c: return",
        selectedName,
        shortcutNames[0], shortcutNames[1], shortcutNames[2], shortcutNames[3], shortcutNames[4]);
      lv_label_set_text(shortcutLabel, screenText);
    }

    void createShortcutScreen() {
      shortcutScreen = lv_obj_create(nullptr);
      lv_obj_set_style_bg_color(shortcutScreen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(shortcutScreen, LV_OPA_COVER, LV_PART_MAIN);

      shortcutLabel = lv_label_create(shortcutScreen);
      lv_obj_set_style_text_color(shortcutLabel, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(shortcutLabel, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_align(shortcutLabel, LV_ALIGN_TOP_LEFT, 8, 3);
      updateShortcutScreen();
    }

    void createBatteryIndicator() {
      lv_obj_set_pos(objects.label_date, 50, 36);
      lv_obj_set_width(objects.label_date, 85);

      lv_obj_set_pos(objects.label_current, 3, 51);
      lv_obj_set_size(objects.label_current, 45, LV_SIZE_CONTENT);
      lv_obj_set_style_text_font(objects.label_current, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_letter_space(objects.label_current, 0, LV_PART_MAIN);

      batteryLabel = lv_label_create(objects.main);
      lv_obj_set_pos(batteryLabel, 50, 51);
      lv_obj_set_size(batteryLabel, 43, LV_SIZE_CONTENT);
      lv_label_set_long_mode(batteryLabel, LV_LABEL_LONG_CLIP);
      lv_obj_set_style_text_font(batteryLabel, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_color(batteryLabel, lv_color_hex(0xffffd82a), LV_PART_MAIN);
      lv_label_set_text(batteryLabel, "B:--");

      lv_obj_set_pos(objects.label_freeheap, 96, 51);
      lv_obj_set_size(objects.label_freeheap, 39, LV_SIZE_CONTENT);
      lv_obj_set_style_text_font(objects.label_freeheap, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_letter_space(objects.label_freeheap, 0, LV_PART_MAIN);
    }

    void openShortcutConfig() {
      selectedEffect = strip.getSegment(strip.getMainSegmentId()).mode;
      shortcutMode = true;
      updateShortcutScreen();
      lv_scr_load_anim(shortcutScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    void assignEffectShortcut(uint8_t index) {
      effectShortcuts[index] = selectedEffect;
      if (shortcutPreferencesReady) {
        char keyName[5];
        snprintf(keyName, sizeof(keyName), "key%u", index + 1);
        shortcutPreferences.putUChar(keyName, selectedEffect);
      }
      updateShortcutScreen();
    }

    void activateEffectShortcut(uint8_t index) {
      uint8_t effect = effectShortcuts[index];
      if (effect == UNASSIGNED_EFFECT || effect >= strip.getModeCount()) return;

      strip.getSegment(strip.getMainSegmentId()).setMode(effect);
      setValuesFromMainSeg();
      stateChanged = true;
    }

    // Keyboard handling
    void handleKeyboard() {
      M5Cardputer.update();

      if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

        for (auto key : status.word) {
          uint8_t newFx;

          if (shortcutMode) {
            if (key == 'c' || key == 'C') {
              shortcutMode = false;
              loadScreen(SCREEN_ID_MAIN);
            } else if (key == '.') {
              selectedEffect = (selectedEffect + strip.getModeCount() - 1) % strip.getModeCount();
              updateShortcutScreen();
            } else if (key == ';') {
              selectedEffect = (selectedEffect + 1) % strip.getModeCount();
              updateShortcutScreen();
            } else if (key >= '1' && key <= '5') {
              assignEffectShortcut(key - '1');
            }
            continue;
          }

          switch (key) {
            case 'h':
            case 'H':
              helpVisible = !helpVisible;
              if (helpVisible) {
                lv_scr_load_anim(helpScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
              } else {
                loadScreen(SCREEN_ID_MAIN);
              }
              break;

            case 'f':
            case 'F':
              toggleOnOff();
              stateUpdated(CALL_MODE_BUTTON);
              break;

            case 'c':
            case 'C':
              openShortcutConfig();
              break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
              activateEffectShortcut(key - '1');
              break;

            case ';':  // FX up (next effect)
              newFx = (strip.getSegment(strip.getMainSegmentId()).mode + 1) % strip.getModeCount();
              strip.getSegment(strip.getMainSegmentId()).setMode(newFx);
              setValuesFromMainSeg();
              stateChanged = true;
              break;

            case '.':  // FX down (previous effect)
              newFx = strip.getSegment(strip.getMainSegmentId()).mode;
              if (newFx == 0) {
                newFx = strip.getModeCount() - 1;
              } else {
                newFx--;
              }
              strip.getSegment(strip.getMainSegmentId()).setMode(newFx);
              setValuesFromMainSeg();
              stateChanged = true;
              break;

            case ',':  // Brightness down (left)
              if (bri > BRIGHTNESS_STEP) {
                bri -= BRIGHTNESS_STEP;
              } else {
                bri = 0;
              }
              stateUpdated(CALL_MODE_BUTTON);
              break;

            case '/':  // Brightness up (right)
              if (bri < 255 - BRIGHTNESS_STEP) {
                bri += BRIGHTNESS_STEP;
              } else {
                bri = 255;
              }
              stateUpdated(CALL_MODE_BUTTON);
              break;
          }
        }
      }
    }

  public:
    void setup() override {
      // Initialize M5Cardputer with display and keyboard
      auto cfg = M5.config();
      M5Cardputer.begin(cfg, true);  // true = enable keyboard

      // Configure display
      M5Cardputer.Display.setRotation(1);  // Landscape for Cardputer
      M5Cardputer.Display.fillScreen(TFT_BLACK);
      M5Cardputer.Display.setBrightness(128);

      // Initialize LVGL with M5Cardputer's display
      lvgl_setup(&M5Cardputer.Display);

      // Initialize UI (creates screens and loads main screen)
      ui_init();
      createBatteryIndicator();
      createHelpScreen();
      createShortcutScreen();

      shortcutPreferencesReady = shortcutPreferences.begin("cardkeys", false);
      if (shortcutPreferencesReady) {
        for (uint8_t index = 0; index < 5; index++) {
          char keyName[5];
          snprintf(keyName, sizeof(keyName), "key%u", index + 1);
          effectShortcuts[index] = shortcutPreferences.getUChar(keyName, UNASSIGNED_EFFECT);
        }
        updateShortcutScreen();
      }

      // Create FreeRTOS task for LVGL tick
      xTaskCreatePinnedToCore(lv_tick_task, "lv_tick_task", 4096, NULL, 5, NULL, 1);

      initDone = true;
    }

    void loop() override {
      if (!initDone) return;

      // Handle keyboard input
      handleKeyboard();

      // Handle LVGL timer
      lv_timer_handler();

      unsigned long now = millis();
      if (now - lastRefresh < REFRESH_RATE_MS) return;
      lastRefresh = now;

      updateDisplay();
    }

    void updateDisplay() {
      // Update WiFi SSID
      if (WLED_CONNECTED) {
        lv_label_set_text(objects.label_ssid, WiFi.SSID().c_str());
      } else if (apActive) {
        lv_label_set_text(objects.label_ssid, "AP: WLED-AP");
      } else {
        lv_label_set_text(objects.label_ssid, "Disconnected");
      }

      // Update IP Address
      if (WLED_CONNECTED) {
        lv_label_set_text(objects.label_ip, WiFi.localIP().toString().c_str());
      } else if (apActive) {
        lv_label_set_text(objects.label_ip, "4.3.2.1");
      } else {
        lv_label_set_text(objects.label_ip, "---");
      }

      // Update LED Pin
      if (BusManager::getNumBusses() > 0) {
        Bus* bus = BusManager::getBus(0);
        if (bus) {
          uint8_t pins[5];
          uint8_t numPins = bus->getPins(pins);
          if (numPins > 0) {
            char pinStr[16];
            snprintf(pinStr, sizeof(pinStr), "GPIO %d", pins[0]);
            lv_label_set_text(objects.label_ledpin, pinStr);
          }
        }
      }

      // Update LED Count
      char ledCountStr[16];
      snprintf(ledCountStr, sizeof(ledCountStr), "%d", strip.getLengthTotal());
      lv_label_set_text(objects.label_lednos, ledCountStr);

      // Update Brightness
      char briStr[20];
      snprintf(briStr, sizeof(briStr), "%d (%d%%)", bri, (bri * 100) / 255);
      lv_label_set_text(objects.label_brightness, briStr);
      lv_bar_set_value(objects.bar_brightness, (bri * 100) / 255, LV_ANIM_OFF);

      // Update Effect name
      char fxName[20];
      strncpy(fxName, strip.getModeData(strip.getMainSegment().mode), 19);
      fxName[19] = '\0';
      char* atSign = strchr(fxName, '@');
      if (atSign) *atSign = '\0';
      lv_label_set_text(objects.label_fx, fxName);

      // Update logo state (ON/OFF based on brightness)
      if (bri > 0) {
        lv_obj_clear_state(objects.image_logo, LV_STATE_DISABLED);
      } else {
        lv_obj_add_state(objects.image_logo, LV_STATE_DISABLED);
      }

      // Update Time
      char timeStr[10];
      snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour(localTime), minute(localTime));
      lv_label_set_text(objects.label_time, timeStr);

      // Update Date
      char dateStr[12];
      snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", day(localTime), month(localTime), year(localTime));
      lv_label_set_text(objects.label_date, dateStr);

      // Update Estimated Current (mA)
      uint32_t estimatedCurrent = BusManager::currentMilliamps();
      char currentStr[16];
      if (estimatedCurrent >= 1000) {
        snprintf(currentStr, sizeof(currentStr), "%.1fA", estimatedCurrent / 1000.0f);
      } else {
        snprintf(currentStr, sizeof(currentStr), "%lumA", estimatedCurrent);
      }
      lv_label_set_text(objects.label_current, currentStr);

      // Update Free Heap
      uint32_t freeHeap = ESP.getFreeHeap();
      char heapStr[16];
      snprintf(heapStr, sizeof(heapStr), "%luKB", (unsigned long)((freeHeap + 512) / 1024));
      lv_label_set_text(objects.label_freeheap, heapStr);

      // Update battery charge estimate; M5Unified reports negative values when unavailable.
      int32_t batteryLevel = M5Cardputer.Power.getBatteryLevel();
      char batteryStr[12];
      if (batteryLevel >= 0) {
        snprintf(batteryStr, sizeof(batteryStr), "B:%ld%%", (long)batteryLevel);
      } else {
        snprintf(batteryStr, sizeof(batteryStr), "B:--");
      }
      lv_label_set_text(batteryLabel, batteryStr);

      // Call UI tick for any animations
      ui_tick();
    }

    uint16_t getId() override {
      return USERMOD_ID_UNSPECIFIED;
    }
};

// Create instance and register
static CardputerDisplayUsermod cardputerDisplay;
REGISTER_USERMOD(cardputerDisplay);
