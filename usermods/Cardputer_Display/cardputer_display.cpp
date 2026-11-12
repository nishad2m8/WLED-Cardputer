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
    bool pinReturnToHelp = false;
    bool shortcutPreferencesReady = false;
    uint8_t effectShortcuts[5] = {
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT, UNASSIGNED_EFFECT,
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT
    };
    uint8_t selectedEffect = 0;
    size_t selectedPinOption = 0;
    Preferences shortcutPreferences;
    lv_obj_t* helpScreen = nullptr;
    lv_obj_t* helpContent = nullptr;
    lv_obj_t* shortcutScreen = nullptr;
    lv_obj_t* selectedEffectLabel = nullptr;
    lv_obj_t* effectAssignmentLabels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* pinScreen = nullptr;
    lv_obj_t* pinList = nullptr;
    lv_obj_t* pinFooter = nullptr;
    lv_obj_t* batteryLabel = nullptr;
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
      addRow("g", "Configure LED output pin");
      addSection("EFFECTS");
      addRow("c", "Configure effect keys");
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
      lv_label_set_text(pinFooter, ";/. Move   Enter Set   G Back");
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
      pinReturnToHelp = helpVisible;
      helpVisible = false;
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
      if (pinReturnToHelp) {
        helpVisible = true;
        lv_scr_load_anim(helpScreen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
      } else {
        loadScreen(SCREEN_ID_MAIN);
      }
    }

    bool queueOutputPinChange(uint8_t gpio) {
      Bus* firstBus = BusManager::getBus(0);
      if (!firstBus || !firstBus->isDigital() || firstBus->is2Pin()) return false;
      uint8_t currentPins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      if (firstBus->getPins(currentPins) == 0) return false;
      if (gpio == currentPins[0]) return true;
      if (!PinManager::isPinOk(gpio, true) || PinManager::isPinAllocated(gpio)) return false;

      busConfigs.clear();
      for (size_t index = 0; index < BusManager::getNumBusses(); index++) {
        Bus* bus = BusManager::getBus(index);
        if (!bus) continue;
        uint8_t pins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        bus->getPins(pins);
        if (index == 0) pins[0] = gpio;
        uint8_t type = bus->getType() | (bus->isOffRefreshRequired() ? 0x80 : 0);
        busConfigs.emplace_back(type, pins, bus->getStart(), bus->getLength(), bus->getColorOrder(),
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
      if (!queueOutputPinChange(outputPinOptions[selectedPinOption].gpio)) {
        lv_label_set_text(pinFooter, "Pin unavailable   G Back");
        return;
      }
      closeOutputPinMenu();
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
      lv_label_set_text(footer, "1-5 Assign   C Back");
      lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -2);
      lv_obj_set_style_text_color(footer, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(footer, &ui_font_br_13, LV_PART_MAIN);
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

          if (pinConfigMode) {
            if (key == ';') {
              selectedPinOption = (selectedPinOption + 1) % outputPinOptions.size();
              updateOutputPinSelection();
            } else if (key == '.') {
              selectedPinOption = (selectedPinOption + outputPinOptions.size() - 1) % outputPinOptions.size();
              updateOutputPinSelection();
            } else if (key == 'g' || key == 'G' || key == 'h' || key == 'H') {
              closeOutputPinMenu();
            }
            continue;
          }

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

            case 'g':
            case 'G':
              openOutputPinMenu();
              break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
              activateEffectShortcut(key - '1');
              break;

            case ';':  // Scroll help up or select the next effect
              if (helpVisible) {
                lv_obj_scroll_by_bounded(helpContent, 0, 24, LV_ANIM_ON);
              } else {
                newFx = (strip.getSegment(strip.getMainSegmentId()).mode + 1) % strip.getModeCount();
                strip.getSegment(strip.getMainSegmentId()).setMode(newFx);
                setValuesFromMainSeg();
                stateChanged = true;
              }
              break;

            case '.':  // Scroll help down or select the previous effect
              if (helpVisible) {
                lv_obj_scroll_by_bounded(helpContent, 0, -24, LV_ANIM_ON);
              } else {
                newFx = strip.getSegment(strip.getMainSegmentId()).mode;
                if (newFx == 0) {
                  newFx = strip.getModeCount() - 1;
                } else {
                  newFx--;
                }
                strip.getSegment(strip.getMainSegmentId()).setMode(newFx);
                setValuesFromMainSeg();
                stateChanged = true;
              }
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

        if (pinConfigMode && status.enter) applySelectedOutputPin();
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
