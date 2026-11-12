#include "wled.h"
#include <M5Cardputer.h>
#include <Preferences.h>
#include <lvgl.h>
#include "lvgl_setup.h"
#include "ui/ui.h"
#include "ui/screens.h"

extern "C" {
  extern const lv_font_t ui_font_br_13;
  extern const lv_font_t ui_font_br_15;
  extern const lv_font_t ui_font_bsb_15;
  extern const lv_font_t ui_font_bsb_18;
}

// Brightness step for keyboard control (0-255 scale)
#define BRIGHTNESS_STEP 25

class CardputerDisplayUsermod : public Usermod {
  private:
    struct OutputPinOption {
      uint8_t gpio;
      String label;
    };

    static constexpr uint8_t UNASSIGNED_EFFECT = 0xFF;
    bool initDone = false;
    bool helpVisible = false;
    bool shortcutMode = false;
    bool pinConfigMode = false;
    bool configMenuMode = false;
    bool configEditMode = false;
    bool shortcutPreferencesReady = false;
    uint8_t effectShortcuts[5] = {
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT, UNASSIGNED_EFFECT,
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT
    };
    uint8_t selectedEffect = 0;
    uint8_t selectedConfigOption = 0;
    size_t selectedPinOption = 0;
    uint16_t configEditValue = 0;
    char ledCountInput[6] = {0};
    uint8_t ledCountInputLength = 0;
    bool configInputError = false;
    Preferences shortcutPreferences;
    lv_obj_t* helpScreen = nullptr;
    lv_obj_t* helpContent = nullptr;
    lv_obj_t* shortcutScreen = nullptr;
    lv_obj_t* selectedEffectLabel = nullptr;
    lv_obj_t* effectAssignmentLabels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* configMenuScreen = nullptr;
    lv_obj_t* configOptionRows[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* configOptionLabels[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* configMenuFooter = nullptr;
    lv_obj_t* pinScreen = nullptr;
    lv_obj_t* pinList = nullptr;
    lv_obj_t* pinFooter = nullptr;
    lv_obj_t* batteryLabel = nullptr;
    lv_obj_t* batteryLevelBar = nullptr;
    std::vector<OutputPinOption> outputPinOptions;
    std::vector<lv_obj_t*> outputPinRows;
    unsigned long lastRefresh = 0;
    unsigned long lastTickTime = 0;
    static const unsigned long REFRESH_RATE_MS = 500;

    void createHelpScreen() {
      helpScreen = lv_obj_create(nullptr);
      lv_obj_remove_flag(helpScreen, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(helpScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_border_width(helpScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(helpScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_color(helpScreen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(helpScreen, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_t* title = lv_label_create(helpScreen);
      lv_label_set_text(title, "KEYBOARD HELP");
      lv_obj_set_pos(title, 6, 2);
      lv_obj_set_style_text_color(title, lv_color_hex(0xffffd82a), LV_PART_MAIN);
      lv_obj_set_style_text_font(title, &ui_font_bsb_18, LV_PART_MAIN);

      lv_obj_t* topDivider = lv_obj_create(helpScreen);
      lv_obj_set_pos(topDivider, 6, 24);
      lv_obj_set_size(topDivider, 228, 2);
      lv_obj_set_style_bg_color(topDivider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(topDivider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(topDivider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(topDivider, 0, LV_PART_MAIN);

      helpContent = lv_obj_create(helpScreen);
      lv_obj_set_pos(helpContent, 6, 29);
      lv_obj_set_size(helpContent, 228, 84);
      lv_obj_set_style_bg_opa(helpContent, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(helpContent, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(helpContent, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(helpContent, 0, LV_PART_MAIN);
      lv_obj_set_scroll_dir(helpContent, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(helpContent, LV_SCROLLBAR_MODE_OFF);

      int32_t contentY = 0;
      auto addSection = [&](const char* text) {
        lv_obj_t* label = lv_label_create(helpContent);
        lv_label_set_text(label, text);
        lv_obj_set_pos(label, 0, contentY);
        lv_obj_set_style_text_color(label, lv_color_hex(0xffde20e4), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &ui_font_bsb_15, LV_PART_MAIN);
        contentY += 17;
      };
      auto addRow = [&](const char* key, const char* description) {
        lv_obj_t* keyLabel = lv_label_create(helpContent);
        lv_label_set_text(keyLabel, key);
        lv_obj_set_pos(keyLabel, 0, contentY);
        lv_obj_set_width(keyLabel, 60);
        lv_obj_set_style_text_color(keyLabel, lv_color_hex(0xffffd82a), LV_PART_MAIN);
        lv_obj_set_style_text_font(keyLabel, &ui_font_bsb_15, LV_PART_MAIN);

        lv_obj_t* descriptionLabel = lv_label_create(helpContent);
        lv_label_set_text(descriptionLabel, description);
        lv_obj_set_pos(descriptionLabel, 62, contentY);
        lv_obj_set_width(descriptionLabel, 166);
        lv_obj_set_style_text_color(descriptionLabel, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(descriptionLabel, &ui_font_br_15, LV_PART_MAIN);
        contentY += 16;
      };

      addSection("LED CONTROL");
      addRow("BTN G0", "Toggle LEDs");
      addRow("f", "Toggle LEDs");
      addSection("NETWORK");
      addRow("w", "Toggle WiFi");
      addSection("EFFECTS");
      addRow("c", "LED and effect setup");
      addRow("1-5", "Activate assigned effect");
      addRow("; / .", "Next / previous effect");
      addSection("BRIGHTNESS");
      addRow(", /", "Decrease / increase");
      addSection("HELP");
      addRow("; / .", "Scroll up / down");

      lv_obj_t* bottomDivider = lv_obj_create(helpScreen);
      lv_obj_set_pos(bottomDivider, 6, 117);
      lv_obj_set_size(bottomDivider, 228, 1);
      lv_obj_set_style_bg_color(bottomDivider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bottomDivider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(bottomDivider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(bottomDivider, 0, LV_PART_MAIN);

      lv_obj_t* footer = lv_label_create(helpScreen);
      lv_label_set_text(footer, "; Up   . Down   H Close");
      lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -2);
      lv_obj_set_style_text_color(footer, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(footer, &ui_font_br_13, LV_PART_MAIN);
    }

    uint8_t getCurrentOutputPin() {
      Bus* bus = BusManager::getBus(0);
      if (!bus) return 0xFF;
      uint8_t pins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      return bus->getPins(pins) > 0 ? pins[0] : 0xFF;
    }

    uint16_t getCurrentOutputLength() {
      Bus* bus = BusManager::getBus(0);
      return bus ? bus->getLength() : 0;
    }

    uint16_t getMaxOutputLength() {
      Bus* bus = BusManager::getBus(0);
      if (!bus || bus->getStart() >= MAX_LEDS) return 0;
      return std::min((uint16_t)MAX_LEDS_PER_BUS, (uint16_t)(MAX_LEDS - bus->getStart()));
    }

    bool isCardputerPeripheralPin(uint8_t gpio) const {
      return gpio == 0 || (gpio >= 3 && gpio <= 15) || gpio == 21 ||
             (gpio >= 33 && gpio <= 46);
    }

    void addOutputPinOption(uint8_t gpio, const char* label, uint8_t currentPin, bool allowBusy = false) {
      if (!PinManager::isPinOk(gpio, true)) return;
      if (!allowBusy && gpio != currentPin && PinManager::isPinAllocated(gpio)) return;
      for (const auto& option : outputPinOptions) {
        if (option.gpio == gpio) return;
      }
      outputPinOptions.push_back({gpio, String(label)});
    }

    void updateOutputPinSelection() {
      for (size_t index = 0; index < outputPinRows.size(); index++) {
        lv_obj_t* row = outputPinRows[index];
        lv_obj_t* label = lv_obj_get_child(row, 0);
        bool selected = index == selectedPinOption;
        lv_obj_set_style_bg_color(row, lv_color_hex(0xffde20e4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, selected ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
      }
      if (!outputPinRows.empty()) {
        lv_obj_scroll_to_view(outputPinRows[selectedPinOption], LV_ANIM_ON);
      }
        lv_label_set_text(pinFooter, "; Up  . Down  Enter Set  C Back");
    }

    void createOutputPinScreen() {
      pinScreen = lv_obj_create(nullptr);
      lv_obj_remove_flag(pinScreen, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(pinScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_border_width(pinScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(pinScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_color(pinScreen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(pinScreen, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_t* title = lv_label_create(pinScreen);
      lv_label_set_text(title, "LED OUTPUT PIN");
      lv_obj_set_pos(title, 6, 2);
      lv_obj_set_style_text_color(title, lv_color_hex(0xffffd82a), LV_PART_MAIN);
      lv_obj_set_style_text_font(title, &ui_font_bsb_18, LV_PART_MAIN);

      lv_obj_t* divider = lv_obj_create(pinScreen);
      lv_obj_set_pos(divider, 6, 23);
      lv_obj_set_size(divider, 228, 2);
      lv_obj_set_style_bg_color(divider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);

      pinList = lv_obj_create(pinScreen);
      lv_obj_set_pos(pinList, 6, 28);
      lv_obj_set_size(pinList, 228, 84);
      lv_obj_set_style_bg_opa(pinList, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(pinList, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(pinList, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(pinList, 0, LV_PART_MAIN);
      lv_obj_set_scroll_dir(pinList, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(pinList, LV_SCROLLBAR_MODE_OFF);

      uint8_t currentPin = getCurrentOutputPin();
      addOutputPinOption(21, "Internal RGB LED (GPIO 21)", currentPin, true);
      addOutputPinOption(1, "Port A G1 (GPIO 1)", currentPin, true);
      addOutputPinOption(2, "Port A G2 (GPIO 2)", currentPin);

      for (uint8_t gpio = 0; gpio < WLED_NUM_PINS; gpio++) {
        if (gpio == 1 || gpio == 2 || gpio == 21 || isCardputerPeripheralPin(gpio)) continue;
        char label[20];
        snprintf(label, sizeof(label), "GPIO %u", gpio);
        addOutputPinOption(gpio, label, currentPin);
      }

      for (size_t index = 0; index < outputPinOptions.size(); index++) {
        lv_obj_t* row = lv_obj_create(pinList);
        lv_obj_set_pos(row, 0, index * 17);
        lv_obj_set_size(row, 228, 16);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);

        lv_obj_t* label = lv_label_create(row);
        lv_label_set_text(label, outputPinOptions[index].label.c_str());
        lv_obj_set_width(label, 228);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(label, &ui_font_br_15, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        outputPinRows.push_back(row);
      }

      lv_obj_t* bottomDivider = lv_obj_create(pinScreen);
      lv_obj_set_pos(bottomDivider, 6, 117);
      lv_obj_set_size(bottomDivider, 228, 1);
      lv_obj_set_style_bg_color(bottomDivider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bottomDivider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(bottomDivider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(bottomDivider, 0, LV_PART_MAIN);

      pinFooter = lv_label_create(pinScreen);
      lv_obj_align(pinFooter, LV_ALIGN_BOTTOM_MID, 0, -2);
      lv_obj_set_style_text_color(pinFooter, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(pinFooter, &ui_font_br_13, LV_PART_MAIN);
      updateOutputPinSelection();
    }

    void openOutputPinMenu() {
      if (outputPinOptions.empty()) return;
      helpVisible = false;
      configMenuMode = false;
      configEditMode = false;
      pinConfigMode = true;
      selectedPinOption = 0;
      uint8_t currentPin = getCurrentOutputPin();
      for (size_t index = 0; index < outputPinOptions.size(); index++) {
        if (outputPinOptions[index].gpio == currentPin) {
          selectedPinOption = index;
          break;
        }
      }
      updateOutputPinSelection();
      lv_scr_load_anim(pinScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    void closeOutputPinMenu() {
      pinConfigMode = false;
      configMenuMode = true;
      updateConfigMenu();
      lv_scr_load_anim(configMenuScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    bool queueOutputBusChange(uint8_t gpio, uint16_t ledCount) {
      Bus* firstBus = BusManager::getBus(0);
      if (!firstBus || !firstBus->isDigital() || firstBus->is2Pin()) return false;
      uint8_t currentPins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      if (firstBus->getPins(currentPins) == 0) return false;
      uint16_t start = firstBus->getStart();
      if (start >= MAX_LEDS) return false;
      uint16_t maxLedCount = getMaxOutputLength();
      if (ledCount == 0 || ledCount > maxLedCount) return false;
      bool pinChanged = gpio != currentPins[0];
      if (pinChanged && (!PinManager::isPinOk(gpio, true) || PinManager::isPinAllocated(gpio))) return false;
      if (!pinChanged && ledCount == firstBus->getLength()) return true;

      busConfigs.clear();
      for (size_t index = 0; index < BusManager::getNumBusses(); index++) {
        Bus* bus = BusManager::getBus(index);
        if (!bus) continue;
        uint8_t pins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        bus->getPins(pins);
        if (index == 0) pins[0] = gpio;
        uint16_t length = index == 0 ? ledCount : bus->getLength();
        uint8_t type = bus->getType() | (bus->isOffRefreshRequired() ? 0x80 : 0);
        busConfigs.emplace_back(type, pins, bus->getStart(), length, bus->getColorOrder(),
                                bus->isReversed(), bus->skippedLeds(), bus->getAutoWhiteMode(),
                                bus->getFrequency(), bus->getLEDCurrent(), bus->getMaxCurrent(),
                                bus->getCustomText());
      }
      if (busConfigs.empty()) return false;
      doInitBusses = true;
      return true;
    }

    void applySelectedOutputPin() {
      if (selectedPinOption >= outputPinOptions.size()) return;
      if (!queueOutputBusChange(outputPinOptions[selectedPinOption].gpio, getCurrentOutputLength())) {
        lv_label_set_text(pinFooter, "Pin unavailable   C Back");
        return;
      }
      closeOutputPinMenu();
    }

    void updateConfigMenu() {
      char optionText[48];
      lv_label_set_text(configOptionLabels[0], "Effect key shortcuts");
      snprintf(optionText, sizeof(optionText), "Output pin: GPIO %u", getCurrentOutputPin());
      lv_label_set_text(configOptionLabels[1], optionText);

      if (configEditMode && selectedConfigOption == 2) {
        snprintf(optionText, sizeof(optionText), "LED count: >%s", ledCountInput);
      } else {
        snprintf(optionText, sizeof(optionText), "LED count: %u", getCurrentOutputLength());
      }
      lv_label_set_text(configOptionLabels[2], optionText);

      uint16_t maxCurrent = configEditMode && selectedConfigOption == 3 ? configEditValue : BusManager::ablMilliampsMax();
      if (maxCurrent == 0) {
        lv_label_set_text(configOptionLabels[3], "Max current: Off");
      } else {
        snprintf(optionText, sizeof(optionText), "Max current: %u mA", maxCurrent);
        lv_label_set_text(configOptionLabels[3], optionText);
      }

      for (uint8_t index = 0; index < 4; index++) {
        bool selected = index == selectedConfigOption;
        lv_obj_set_style_bg_color(configOptionRows[index],
          selected && configEditMode ? lv_color_hex(0xffffd82a) : lv_color_hex(0xffde20e4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(configOptionRows[index], selected ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_color(configOptionLabels[index], selected ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
      }

      if (configEditMode && selectedConfigOption == 2) {
        if (configInputError) {
          char errorText[48];
          snprintf(errorText, sizeof(errorText), "Valid range: 1-%u LEDs   C Cancel", getMaxOutputLength());
          lv_label_set_text(configMenuFooter, errorText);
        } else {
          lv_label_set_text(configMenuFooter, "Digits  Enter Save  Del Erase  C Cancel");
        }
      } else if (configEditMode) {
        lv_label_set_text(configMenuFooter, ", / Change   Enter Save   C Cancel");
      } else {
          lv_label_set_text(configMenuFooter, "; Up  . Down  Enter Select  C Back");
      }
    }

    void createConfigMenuScreen() {
      configMenuScreen = lv_obj_create(nullptr);
      lv_obj_remove_flag(configMenuScreen, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(configMenuScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_border_width(configMenuScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(configMenuScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_color(configMenuScreen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(configMenuScreen, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_t* title = lv_label_create(configMenuScreen);
      lv_label_set_text(title, "C MENU");
      lv_obj_set_pos(title, 6, 2);
      lv_obj_set_style_text_color(title, lv_color_hex(0xffffd82a), LV_PART_MAIN);
      lv_obj_set_style_text_font(title, &ui_font_bsb_18, LV_PART_MAIN);

      lv_obj_t* divider = lv_obj_create(configMenuScreen);
      lv_obj_set_pos(divider, 6, 23);
      lv_obj_set_size(divider, 228, 2);
      lv_obj_set_style_bg_color(divider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);

      for (uint8_t index = 0; index < 4; index++) {
        configOptionRows[index] = lv_obj_create(configMenuScreen);
        lv_obj_set_pos(configOptionRows[index], 6, 31 + index * 20);
        lv_obj_set_size(configOptionRows[index], 228, 18);
        lv_obj_remove_flag(configOptionRows[index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(configOptionRows[index], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(configOptionRows[index], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(configOptionRows[index], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(configOptionRows[index], 0, LV_PART_MAIN);

        configOptionLabels[index] = lv_label_create(configOptionRows[index]);
        lv_obj_set_pos(configOptionLabels[index], 3, 0);
        lv_obj_set_width(configOptionLabels[index], 222);
        lv_label_set_long_mode(configOptionLabels[index], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(configOptionLabels[index], &ui_font_br_15, LV_PART_MAIN);
        lv_obj_set_style_text_color(configOptionLabels[index], lv_color_white(), LV_PART_MAIN);
      }

      lv_obj_t* bottomDivider = lv_obj_create(configMenuScreen);
      lv_obj_set_pos(bottomDivider, 6, 117);
      lv_obj_set_size(bottomDivider, 228, 1);
      lv_obj_set_style_bg_color(bottomDivider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bottomDivider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(bottomDivider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(bottomDivider, 0, LV_PART_MAIN);

      configMenuFooter = lv_label_create(configMenuScreen);
      lv_obj_align(configMenuFooter, LV_ALIGN_BOTTOM_MID, 0, -2);
      lv_obj_set_style_text_color(configMenuFooter, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(configMenuFooter, &ui_font_br_13, LV_PART_MAIN);
      updateConfigMenu();
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
      char selectedText[48];
      getEffectName(selectedEffect, selectedName, sizeof(selectedName));
      snprintf(selectedText, sizeof(selectedText), "Selected: %s", selectedName);
      lv_label_set_text(selectedEffectLabel, selectedText);

      for (uint8_t index = 0; index < 5; index++) {
        char shortcutName[32];
        if (effectShortcuts[index] == UNASSIGNED_EFFECT) {
          strcpy(shortcutName, "--");
        } else {
          getEffectName(effectShortcuts[index], shortcutName, sizeof(shortcutName));
        }
        lv_label_set_text(effectAssignmentLabels[index], shortcutName);
      }
    }

    void createShortcutScreen() {
      shortcutScreen = lv_obj_create(nullptr);
      lv_obj_remove_flag(shortcutScreen, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_pad_all(shortcutScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_border_width(shortcutScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(shortcutScreen, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_color(shortcutScreen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(shortcutScreen, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_t* title = lv_label_create(shortcutScreen);
      lv_label_set_text(title, "SET EFFECT KEYS");
      lv_obj_set_pos(title, 6, 2);
      lv_obj_set_style_text_color(title, lv_color_hex(0xffffd82a), LV_PART_MAIN);
      lv_obj_set_style_text_font(title, &ui_font_bsb_18, LV_PART_MAIN);

      lv_obj_t* topDivider = lv_obj_create(shortcutScreen);
      lv_obj_set_pos(topDivider, 6, 23);
      lv_obj_set_size(topDivider, 228, 2);
      lv_obj_set_style_bg_color(topDivider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(topDivider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(topDivider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(topDivider, 0, LV_PART_MAIN);

      selectedEffectLabel = lv_label_create(shortcutScreen);
      lv_obj_set_pos(selectedEffectLabel, 6, 29);
      lv_obj_set_size(selectedEffectLabel, 228, 18);
      lv_label_set_long_mode(selectedEffectLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_obj_set_style_text_color(selectedEffectLabel, lv_color_hex(0xffffd82a), LV_PART_MAIN);
      lv_obj_set_style_text_font(selectedEffectLabel, &ui_font_bsb_15, LV_PART_MAIN);

      lv_obj_t* navigation = lv_label_create(shortcutScreen);
      lv_label_set_text(navigation, "; Next   . Previous effect");
      lv_obj_set_pos(navigation, 6, 48);
      lv_obj_set_style_text_color(navigation, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(navigation, &ui_font_br_13, LV_PART_MAIN);

      lv_obj_t* assignmentsTitle = lv_label_create(shortcutScreen);
      lv_label_set_text(assignmentsTitle, "ASSIGNED EFFECTS");
      lv_obj_set_pos(assignmentsTitle, 6, 62);
      lv_obj_set_style_text_color(assignmentsTitle, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_text_font(assignmentsTitle, &ui_font_bsb_15, LV_PART_MAIN);

      const char* keyLabels[] = {"1", "2", "3", "4", "5"};
      for (uint8_t index = 0; index < 5; index++) {
        uint8_t column = index % 2;
        uint8_t row = index / 2;
        int16_t x = column ? 122 : 6;
        int16_t y = 78 + row * 13;

        lv_obj_t* keyLabel = lv_label_create(shortcutScreen);
        lv_label_set_text(keyLabel, keyLabels[index]);
        lv_obj_set_pos(keyLabel, x, y);
        lv_obj_set_style_text_color(keyLabel, lv_color_hex(0xffffd82a), LV_PART_MAIN);
        lv_obj_set_style_text_font(keyLabel, &ui_font_bsb_15, LV_PART_MAIN);

        effectAssignmentLabels[index] = lv_label_create(shortcutScreen);
        lv_obj_set_pos(effectAssignmentLabels[index], x + 17, y);
        lv_obj_set_width(effectAssignmentLabels[index], column ? 95 : 91);
        lv_label_set_long_mode(effectAssignmentLabels[index], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(effectAssignmentLabels[index], lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(effectAssignmentLabels[index], &ui_font_br_13, LV_PART_MAIN);
      }

      lv_obj_t* bottomDivider = lv_obj_create(shortcutScreen);
      lv_obj_set_pos(bottomDivider, 6, 119);
      lv_obj_set_size(bottomDivider, 228, 1);
      lv_obj_set_style_bg_color(bottomDivider, lv_color_hex(0xffde20e4), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bottomDivider, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(bottomDivider, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(bottomDivider, 0, LV_PART_MAIN);

      lv_obj_t* footer = lv_label_create(shortcutScreen);
      lv_label_set_text(footer, "1-5 Assign   C Menu");
      lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -2);
      lv_obj_set_style_text_color(footer, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(footer, &ui_font_br_13, LV_PART_MAIN);
      updateShortcutScreen();
    }

    void createBatteryIndicator() {
      lv_obj_set_pos(objects.label_date, 50, 36);
      lv_obj_set_width(objects.label_date, 85);

      lv_obj_set_pos(objects.label_current, 3, 51);
      lv_obj_set_size(objects.label_current, 90, LV_SIZE_CONTENT);
      lv_label_set_long_mode(objects.label_current, LV_LABEL_LONG_CLIP);
      lv_obj_set_style_text_font(objects.label_current, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_align(objects.label_current, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
      lv_obj_set_style_text_letter_space(objects.label_current, 0, LV_PART_MAIN);

      batteryLevelBar = lv_bar_create(objects.main);
      lv_obj_set_pos(batteryLevelBar, 201, 4);
      lv_obj_set_size(batteryLevelBar, 39, 12);
      lv_bar_set_range(batteryLevelBar, 0, 100);
      lv_bar_set_value(batteryLevelBar, 0, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(batteryLevelBar, lv_color_hex(0xffb0b0b0), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(batteryLevelBar, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(batteryLevelBar, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(batteryLevelBar, 1, LV_PART_MAIN);
      lv_obj_set_style_bg_color(batteryLevelBar, lv_color_hex(0xff6cff58), LV_PART_INDICATOR);
      lv_obj_set_style_bg_opa(batteryLevelBar, LV_OPA_COVER, LV_PART_INDICATOR);
      lv_obj_set_style_radius(batteryLevelBar, 1, LV_PART_INDICATOR);

      batteryLabel = lv_label_create(objects.main);
      lv_obj_set_pos(batteryLabel, 201, 3);
      lv_obj_set_size(batteryLabel, 39, LV_SIZE_CONTENT);
      lv_label_set_long_mode(batteryLabel, LV_LABEL_LONG_CLIP);
      lv_obj_set_style_text_font(batteryLabel, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_align(batteryLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
      lv_obj_set_style_text_color(batteryLabel, lv_color_black(), LV_PART_MAIN);
      lv_label_set_text(batteryLabel, "--%");

      lv_obj_set_pos(objects.label_freeheap, 96, 51);
      lv_obj_set_size(objects.label_freeheap, 39, LV_SIZE_CONTENT);
      lv_obj_set_style_text_font(objects.label_freeheap, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_letter_space(objects.label_freeheap, 0, LV_PART_MAIN);
    }

    void openShortcutConfig() {
      selectedEffect = strip.getSegment(strip.getMainSegmentId()).mode;
      configMenuMode = false;
      configEditMode = false;
      shortcutMode = true;
      updateShortcutScreen();
      lv_scr_load_anim(shortcutScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    void closeShortcutConfig() {
      shortcutMode = false;
      configMenuMode = true;
      updateConfigMenu();
      lv_scr_load_anim(configMenuScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    void openConfigMenu() {
      helpVisible = false;
      shortcutMode = false;
      pinConfigMode = false;
      configMenuMode = true;
      configEditMode = false;
      selectedConfigOption = 0;
      updateConfigMenu();
      lv_scr_load_anim(configMenuScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    void closeConfigMenu() {
      configMenuMode = false;
      configEditMode = false;
      loadScreen(SCREEN_ID_MAIN);
    }

    void handleConfigMenuEnter() {
      if (configEditMode) {
        bool saved = false;
        if (selectedConfigOption == 2) {
          char* end = nullptr;
          unsigned long enteredCount = strtoul(ledCountInput, &end, 10);
          uint16_t maxLedCount = getMaxOutputLength();
          if (ledCountInputLength == 0 || end == ledCountInput || *end != '\0' ||
              enteredCount < 1 || enteredCount > maxLedCount) {
            configInputError = true;
            updateConfigMenu();
            return;
          }
          saved = queueOutputBusChange(getCurrentOutputPin(), (uint16_t)enteredCount);
          configInputError = !saved;
        } else if (selectedConfigOption == 3) {
          BusManager::setMilliampsMax(configEditValue);
          BusManager::initializeABL();
          configNeedsWrite = true;
          saved = true;
        }
        if (saved) {
          configEditMode = false;
          configInputError = false;
        }
        updateConfigMenu();
        return;
      }

      switch (selectedConfigOption) {
        case 0:
          openShortcutConfig();
          break;
        case 1:
          openOutputPinMenu();
          break;
        case 2:
          ledCountInputLength = 0;
          ledCountInput[0] = '\0';
          configInputError = false;
          configEditMode = true;
          updateConfigMenu();
          break;
        case 3:
          configEditValue = BusManager::ablMilliampsMax();
          configEditMode = true;
          updateConfigMenu();
          break;
      }
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

    void toggleWifi() {
      if (cardputerWifiDisabled) {
        cardputerWifiDisabled = false;
        apActive = false;
        interfacesInited = false;
        WLED::instance().initConnection();
      } else {
        cardputerWifiDisabled = true;
        forceReconnect = false;
        dnsServer.stop();
        apActive = false;
        interfacesInited = false;
        lastReconnectAttempt = millis();
        WiFi.mode(WIFI_OFF);
      }
    }

    // Keyboard handling
    void handleKeyboard() {
      M5Cardputer.update();

      if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        bool helpInputMode = helpVisible;

        for (auto key : status.word) {
          uint8_t newFx;

          if (helpInputMode) {
            if (key == 'h' || key == 'H') {
              helpVisible = false;
              loadScreen(SCREEN_ID_MAIN);
            } else if (key == ';') {
              lv_obj_scroll_by_bounded(helpContent, 0, 24, LV_ANIM_ON);
            } else if (key == '.') {
              lv_obj_scroll_by_bounded(helpContent, 0, -24, LV_ANIM_ON);
            }
            continue;
          }

          if (key == 'w' || key == 'W') {
            toggleWifi();
            continue;
          }

          if (pinConfigMode) {
            if (key == ';') {
              selectedPinOption = (selectedPinOption + outputPinOptions.size() - 1) % outputPinOptions.size();
              updateOutputPinSelection();
            } else if (key == '.') {
              selectedPinOption = (selectedPinOption + 1) % outputPinOptions.size();
              updateOutputPinSelection();
            } else if (key == 'c' || key == 'C') {
              closeOutputPinMenu();
            }
            continue;
          }

          if (shortcutMode) {
            if (key == 'c' || key == 'C') {
              closeShortcutConfig();
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

          if (configMenuMode) {
            if (configEditMode) {
              if (selectedConfigOption == 2 && key >= '0' && key <= '9') {
                if (ledCountInputLength < sizeof(ledCountInput) - 1) {
                  ledCountInput[ledCountInputLength++] = key;
                  ledCountInput[ledCountInputLength] = '\0';
                  configInputError = false;
                } else {
                  configInputError = true;
                }
                updateConfigMenu();
              } else if (selectedConfigOption == 3 && key == ',') {
                if (selectedConfigOption == 2 && configEditValue > 1) configEditValue--;
                else configEditValue = configEditValue > 100 ? configEditValue - 100 : 0;
                updateConfigMenu();
              } else if (selectedConfigOption == 3 && key == '/') {
                if (configEditValue <= 64900) {
                  configEditValue = configEditValue == 0 ? 100 : configEditValue + 100;
                } else {
                  configEditValue = 65000;
                }
                updateConfigMenu();
              } else if (key == 'c' || key == 'C') {
                configEditMode = false;
                configInputError = false;
                updateConfigMenu();
              }
            } else if (key == ';') {
              selectedConfigOption = (selectedConfigOption + 3) % 4;
              updateConfigMenu();
            } else if (key == '.') {
              selectedConfigOption = (selectedConfigOption + 1) % 4;
              updateConfigMenu();
            } else if (key == 'c' || key == 'C') {
              closeConfigMenu();
            }
            continue;
          }

          switch (key) {
            case 'h':
            case 'H':
              helpVisible = true;
              helpInputMode = true;
              lv_scr_load_anim(helpScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
              break;

            case 'f':
            case 'F':
              toggleOnOff();
              stateUpdated(CALL_MODE_BUTTON);
              break;

            case 'c':
            case 'C':
              openConfigMenu();
              break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
              activateEffectShortcut(key - '1');
              break;

            case ';':  // Scroll help up or select the next effect
              newFx = (strip.getSegment(strip.getMainSegmentId()).mode + 1) % strip.getModeCount();
              strip.getSegment(strip.getMainSegmentId()).setMode(newFx);
              setValuesFromMainSeg();
              stateChanged = true;
              break;

            case '.':  // Scroll help down or select the previous effect
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

        if (configMenuMode && configEditMode && selectedConfigOption == 2 && status.del) {
          if (ledCountInputLength > 0) {
            ledCountInput[--ledCountInputLength] = '\0';
            configInputError = false;
            updateConfigMenu();
          }
        }
        if (status.enter) {
          if (pinConfigMode) applySelectedOutputPin();
          else if (configMenuMode) handleConfigMenuEnter();
        }
      }
    }

  public:
    void setup() override {
      // Initialize M5Cardputer with display and keyboard
      auto cfg = M5.config();
      M5Cardputer.begin(cfg, true);  // true = enable keyboard

      // Configure display
      M5Cardputer.Display.setRotation(0);  // M5GFX 0.3.x applies the Cardputer landscape offset
      M5Cardputer.Display.fillScreen(TFT_BLACK);
      M5Cardputer.Display.setBrightness(128);

      // Initialize LVGL with M5Cardputer's display
      lvgl_setup(&M5Cardputer.Display);

      // Initialize UI (creates screens and loads main screen)
      ui_init();
      createBatteryIndicator();
      createHelpScreen();
      createShortcutScreen();
      createOutputPinScreen();
      createConfigMenuScreen();

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
      if (configMenuMode) updateConfigMenu();
    }

    void updateDisplay() {
      const bool wifiEnabled = !cardputerWifiDisabled && WiFi.getMode() != WIFI_OFF;
      lv_color_t wifiTitleColor = wifiEnabled ? lv_color_white() : lv_color_hex(0xff808080);
      lv_obj_set_style_text_color(objects.label_wifi_title, wifiTitleColor, LV_PART_MAIN);
      lv_obj_set_style_text_color(objects.label_ip_title, wifiTitleColor, LV_PART_MAIN);
      if (wifiEnabled) {
        lv_obj_clear_flag(objects.label_ssid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(objects.label_ip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(objects.label_ssid, lv_color_hex(0xff3eadff), LV_PART_MAIN);
        if (WLED_CONNECTED) {
          lv_label_set_text(objects.label_ssid, WiFi.SSID().c_str());
          lv_label_set_text(objects.label_ip, WiFi.localIP().toString().c_str());
        } else if (apActive) {
          lv_label_set_text(objects.label_ssid, "AP: WLED-AP");
          lv_label_set_text(objects.label_ip, "4.3.2.1");
        } else {
          lv_label_set_text(objects.label_ssid, "Disconnected");
          lv_label_set_text(objects.label_ip, "---");
        }
      } else {
        lv_label_set_text(objects.label_ssid, "OFF");
        lv_label_set_text(objects.label_ip, "");
        lv_obj_clear_flag(objects.label_ssid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.label_ip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(objects.label_ssid, lv_color_hex(0xff808080), LV_PART_MAIN);
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
      snprintf(ledCountStr, sizeof(ledCountStr), "%u", getCurrentOutputLength());
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
        snprintf(batteryStr, sizeof(batteryStr), "%ld%%", (long)batteryLevel);
        lv_bar_set_value(batteryLevelBar, constrain(batteryLevel, 0, 100), LV_ANIM_OFF);
        lv_color_t batteryColor = batteryLevel <= 20 ? lv_color_hex(0xffff4040)
          : batteryLevel <= 50 ? lv_color_hex(0xffffd82a) : lv_color_hex(0xff6cff58);
        lv_obj_set_style_bg_color(batteryLevelBar, batteryColor, LV_PART_INDICATOR);
      } else {
        snprintf(batteryStr, sizeof(batteryStr), "--%%");
        lv_bar_set_value(batteryLevelBar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(batteryLevelBar, lv_color_hex(0xff808080), LV_PART_INDICATOR);
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
