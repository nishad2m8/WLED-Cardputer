#include "wled.h"
#include <M5Cardputer.h>
#include <lvgl.h>
#include "lvgl_setup.h"
#include "ui/ui.h"
#include "ui/screens.h"

// Brightness step for keyboard control (0-255 scale)
#define BRIGHTNESS_STEP 25

class CardputerDisplayUsermod : public Usermod {
  private:
    bool initDone = false;
    unsigned long lastRefresh = 0;
    unsigned long lastTickTime = 0;
    static const unsigned long REFRESH_RATE_MS = 500;

    // Keyboard handling
    void handleKeyboard() {
      M5Cardputer.update();

      if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

        for (auto key : status.word) {
          uint8_t newFx;
          switch (key) {
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
      snprintf(heapStr, sizeof(heapStr), "%.1fkb", freeHeap / 1024.0f);
      lv_label_set_text(objects.label_freeheap, heapStr);

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
