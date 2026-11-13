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
static constexpr uint8_t INITIAL_DISPLAY_BRIGHTNESS = 128;

struct CardputerStripTypeOption {
  uint8_t type;
  const char* label;
};

static const CardputerStripTypeOption CARDPUTER_STRIP_TYPES[] = {
  {TYPE_WS2812_RGB, "WS281x RGB"},
  {TYPE_SK6812_RGBW, "SK6812 RGBW"},
  {TYPE_TM1814, "TM1814 RGBW"},
  {TYPE_WS2811_400KHZ, "WS2811 400kHz"},
  {TYPE_TM1829, "TM1829"},
  {TYPE_UCS8903, "UCS8903"},
  {TYPE_APA106, "APA106/PL9823"},
  {TYPE_TM1914, "TM1914"},
  {TYPE_FW1906, "FW1906 RGB+CW"},
  {TYPE_UCS8904, "UCS8904 RGBW"},
  {TYPE_WS2805, "WS2805 RGB+CCT"},
  {TYPE_SM16825, "SM16825 RGB+CCT"},
  {TYPE_WS2812_1CH_X3, "WS2811 White"},
  {TYPE_WS2812_WWA, "WS281x WWA"}
};

static const char* const CARDPUTER_COLOR_ORDERS[] = {"GRB", "RGB", "BRG", "RBG", "GBR", "BGR"};
static const char* const CARDPUTER_WHITE_MODES[] = {"Manual", "Auto brighter", "Auto accurate", "Dual", "White only"};

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
    bool deleteShortcutMode = false;
    bool pinConfigMode = false;
    bool configMenuMode = false;
    bool configEditMode = false;
    bool configInputActive = false;
    bool effectShortcutsLoadedFromConfig = false;
    uint8_t effectShortcuts[5] = {
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT, UNASSIGNED_EFFECT,
      UNASSIGNED_EFFECT, UNASSIGNED_EFFECT
    };
    uint8_t selectedEffect = 0;
    uint8_t selectedConfigOption = 0;
    size_t selectedPinOption = 0;
    uint16_t configEditValue = 0;
    char configInput[6] = {0};
    uint8_t configInputLength = 0;
    bool configInputError = false;
    lv_obj_t* helpScreen = nullptr;
    lv_obj_t* helpContent = nullptr;
    lv_obj_t* shortcutScreen = nullptr;
    lv_obj_t* selectedEffectLabel = nullptr;
    lv_obj_t* effectAssignmentLabels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* configMenuScreen = nullptr;
    lv_obj_t* configMenuList = nullptr;
    lv_obj_t* configOptionRows[12] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* configOptionLabels[12] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
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
    static constexpr uint8_t CONFIG_OPTION_COUNT = 12;

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
      addRow("0 + 1-5", "Delete selected key slot");
      addRow("; / .", "Next / previous effect");
      addSection("LED BRIGHTNESS");
      addRow(", /", "Decrease / increase LEDs");
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

    uint8_t getCurrentStripTypeIndex() {
      Bus* bus = BusManager::getBus(0);
      uint8_t type = bus ? bus->getType() : TYPE_WS2812_RGB;
      for (uint8_t index = 0; index < sizeof(CARDPUTER_STRIP_TYPES) / sizeof(CARDPUTER_STRIP_TYPES[0]); index++) {
        if (CARDPUTER_STRIP_TYPES[index].type == type) return index;
      }
      return 0;
    }

    bool queueOutputBusConfig(uint8_t type, uint8_t gpio, uint16_t ledCount, uint8_t colorOrder,
                              bool reversed, uint8_t skip, uint8_t whiteMode, uint16_t ledCurrent,
                              uint16_t busMaxCurrent) {
      Bus* firstBus = BusManager::getBus(0);
      if (!firstBus || !Bus::isDigital(type) || Bus::is2Pin(type)) return false;
      uint8_t currentPins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      if (firstBus->getPins(currentPins) == 0) return false;
      uint16_t start = firstBus->getStart();
      if (start >= MAX_LEDS) return false;
      uint16_t maxLedCount = getMaxOutputLength();
      if (ledCount == 0 || ledCount > maxLedCount || (skip > 0 && skip >= ledCount)) return false;
      bool pinChanged = gpio != currentPins[0];
      if (pinChanged && (!PinManager::isPinOk(gpio, true) || PinManager::isPinAllocated(gpio))) return false;

      busConfigs.clear();
      for (size_t index = 0; index < BusManager::getNumBusses(); index++) {
        Bus* bus = BusManager::getBus(index);
        if (!bus) continue;
        uint8_t pins[OUTPUT_MAX_PINS] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        bus->getPins(pins);
        if (index == 0) pins[0] = gpio;
        uint8_t busType = bus->getType();
        uint16_t length = bus->getLength();
        uint8_t busColorOrder = bus->getColorOrder();
        bool busReversed = bus->isReversed();
        uint8_t busSkip = bus->skippedLeds();
        uint8_t busWhiteMode = bus->getAutoWhiteMode();
        uint16_t busLedCurrent = bus->getLEDCurrent();
        uint16_t currentBusMax = bus->getMaxCurrent();
        bool refresh = bus->isOffRefreshRequired();
        if (index == 0) {
          busType = type;
          pins[0] = gpio;
          length = ledCount;
          busColorOrder = (busColorOrder & 0xF0) | colorOrder;
          busReversed = reversed;
          busSkip = skip;
          busWhiteMode = Bus::hasWhite(type) ? whiteMode : RGBW_MODE_MANUAL_ONLY;
          busLedCurrent = ledCurrent;
          currentBusMax = busMaxCurrent;
          refresh = Bus::mustRefresh(type);
        }
        busConfigs.emplace_back(busType | (refresh ? 0x80 : 0), pins, bus->getStart(), length,
                                busColorOrder, busReversed, busSkip, busWhiteMode,
                                bus->getFrequency(), busLedCurrent, currentBusMax,
                                bus->getCustomText());
      }
      if (busConfigs.empty()) return false;
      doInitBusses = true;
      return true;
    }

    bool queueOutputBusChange(uint8_t gpio, uint16_t ledCount) {
      Bus* bus = BusManager::getBus(0);
      if (!bus) return false;
      return queueOutputBusConfig(bus->getType(), gpio, ledCount, bus->getColorOrder() & 0x0F,
                                  bus->isReversed(), bus->skippedLeds(), bus->getAutoWhiteMode(),
                                  bus->getLEDCurrent(), bus->getMaxCurrent());
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
      Bus* bus = BusManager::getBus(0);
      uint8_t typeIndex = configEditMode && selectedConfigOption == 1 ? configEditValue : getCurrentStripTypeIndex();
      uint8_t type = CARDPUTER_STRIP_TYPES[typeIndex].type;
      uint8_t colorOrder = configEditMode && selectedConfigOption == 4 ? configEditValue : bus ? bus->getColorOrder() & 0x0F : COL_ORDER_GRB;
      bool reversed = configEditMode && selectedConfigOption == 5 ? configEditValue != 0 : bus && bus->isReversed();
      uint16_t ledCount = configEditMode && selectedConfigOption == 3 && !configInputActive ? configEditValue : getCurrentOutputLength();
      uint8_t skip = configEditMode && selectedConfigOption == 6 && !configInputActive ? configEditValue : bus ? bus->skippedLeds() : 0;
      uint16_t ledCurrent = configEditMode && selectedConfigOption == 7 && !configInputActive ? configEditValue : bus ? bus->getLEDCurrent() : LED_MILLIAMPS_DEFAULT;
      uint16_t busMaxCurrent = configEditMode && selectedConfigOption == 8 && !configInputActive ? configEditValue : bus ? bus->getMaxCurrent() : 0;
      uint16_t maxCurrent = configEditMode && selectedConfigOption == 9 && !configInputActive ? configEditValue : BusManager::ablMilliampsMax();
      uint8_t whiteMode = configEditMode && selectedConfigOption == 10 ? configEditValue : bus ? bus->getAutoWhiteMode() : RGBW_MODE_MANUAL_ONLY;

      lv_label_set_text(configOptionLabels[0], "Effect key shortcuts");
      snprintf(optionText, sizeof(optionText), "LED type: %s", CARDPUTER_STRIP_TYPES[typeIndex].label);
      lv_label_set_text(configOptionLabels[1], optionText);
      snprintf(optionText, sizeof(optionText), "Output pin: GPIO %u", getCurrentOutputPin());
      lv_label_set_text(configOptionLabels[2], optionText);

      if (configEditMode && selectedConfigOption == 3 && configInputActive) {
        snprintf(optionText, sizeof(optionText), "LED count: >%s", configInput);
      } else {
        snprintf(optionText, sizeof(optionText), "LED count: %u", ledCount);
      }
      lv_label_set_text(configOptionLabels[3], optionText);

      snprintf(optionText, sizeof(optionText), "Color order: %s", CARDPUTER_COLOR_ORDERS[colorOrder < 6 ? colorOrder : 0]);
      lv_label_set_text(configOptionLabels[4], optionText);
      snprintf(optionText, sizeof(optionText), "Reverse: %s", reversed ? "On" : "Off");
      lv_label_set_text(configOptionLabels[5], optionText);

      if (configEditMode && selectedConfigOption == 6 && configInputActive) {
        snprintf(optionText, sizeof(optionText), "Skip first: >%s", configInput);
      } else {
        snprintf(optionText, sizeof(optionText), "Skip first: %u", skip);
      }
      lv_label_set_text(configOptionLabels[6], optionText);

      if (configEditMode && selectedConfigOption == 7 && configInputActive) {
        snprintf(optionText, sizeof(optionText), "LED current: >%s mA", configInput);
      } else {
        snprintf(optionText, sizeof(optionText), "LED current: %u mA", ledCurrent);
      }
      lv_label_set_text(configOptionLabels[7], optionText);

      if (configEditMode && selectedConfigOption == 8 && configInputActive) {
        snprintf(optionText, sizeof(optionText), "Bus max current: >%s", configInput);
      } else if (busMaxCurrent == 0) {
        snprintf(optionText, sizeof(optionText), "Bus max current: Off");
      } else {
        snprintf(optionText, sizeof(optionText), "Bus max current: %u mA", busMaxCurrent);
      }
      lv_label_set_text(configOptionLabels[8], optionText);

      if (maxCurrent == 0) {
        lv_label_set_text(configOptionLabels[9], "Max current: Off");
      } else {
        snprintf(optionText, sizeof(optionText), "Max current: %u mA", maxCurrent);
        lv_label_set_text(configOptionLabels[9], optionText);
      }
      if (configEditMode && selectedConfigOption == 9 && configInputActive) {
        snprintf(optionText, sizeof(optionText), "Max current: >%s", configInput);
        lv_label_set_text(configOptionLabels[9], optionText);
      }
      snprintf(optionText, sizeof(optionText), "RGBW mode: %s", Bus::hasWhite(type) ? CARDPUTER_WHITE_MODES[whiteMode < 5 ? whiteMode : 0] : "N/A");
      lv_label_set_text(configOptionLabels[10], optionText);
      lv_label_set_text(configOptionLabels[11], cardputerWifiDisabled ? "WiFi: Off" : "WiFi: On");

      for (uint8_t index = 0; index < CONFIG_OPTION_COUNT; index++) {
        bool selected = index == selectedConfigOption;
        lv_obj_set_style_bg_color(configOptionRows[index],
          selected && configEditMode ? lv_color_hex(0xffffd82a) : lv_color_hex(0xffde20e4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(configOptionRows[index], selected ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_color(configOptionLabels[index], selected ? lv_color_black() : lv_color_white(), LV_PART_MAIN);
      }
      lv_obj_scroll_to_view(configOptionRows[selectedConfigOption], LV_ANIM_ON);

      bool numericOption = selectedConfigOption == 3 || selectedConfigOption == 6 ||
                           selectedConfigOption == 7 || selectedConfigOption == 8 || selectedConfigOption == 9;
      if (configEditMode && numericOption) {
        if (configInputError) {
          lv_label_set_text(configMenuFooter, "Invalid value   C Cancel");
        } else {
          lv_label_set_text(configMenuFooter, "Digits edit  Enter Save  Del Erase  C Cancel");
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

      configMenuList = lv_obj_create(configMenuScreen);
      lv_obj_set_pos(configMenuList, 6, 28);
      lv_obj_set_size(configMenuList, 228, 86);
      lv_obj_set_style_bg_opa(configMenuList, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(configMenuList, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(configMenuList, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(configMenuList, 0, LV_PART_MAIN);
      lv_obj_set_scroll_dir(configMenuList, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(configMenuList, LV_SCROLLBAR_MODE_OFF);

      for (uint8_t index = 0; index < CONFIG_OPTION_COUNT; index++) {
        configOptionRows[index] = lv_obj_create(configMenuList);
        lv_obj_set_pos(configOptionRows[index], 0, index * 19);
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
        lv_obj_set_style_text_font(configOptionLabels[index], &ui_font_br_13, LV_PART_MAIN);
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
      if (deleteShortcutMode) {
        snprintf(selectedText, sizeof(selectedText), "Delete slot: press 1-5");
      } else {
        getEffectName(selectedEffect, selectedName, sizeof(selectedName));
        snprintf(selectedText, sizeof(selectedText), "Selected: %s", selectedName);
      }
      lv_label_set_text(selectedEffectLabel, selectedText);

      for (uint8_t index = 0; index < 5; index++) {
        char shortcutName[32];
        if (effectShortcuts[index] == UNASSIGNED_EFFECT || effectShortcuts[index] >= strip.getModeCount()) {
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
      lv_label_set_text(footer, "1-5 Assign  0+1-5 Clear  C Menu");
      lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -2);
      lv_obj_set_style_text_color(footer, lv_color_white(), LV_PART_MAIN);
      lv_obj_set_style_text_font(footer, &ui_font_br_13, LV_PART_MAIN);
      updateShortcutScreen();
    }

    void createBatteryIndicator() {
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

    void startConfigEdit() {
      Bus* bus = BusManager::getBus(0);
      if (selectedConfigOption == 10 && (!bus || !Bus::hasWhite(bus->getType()))) return;
      configEditMode = true;
      configInputActive = false;
      configInputError = false;
      configInputLength = 0;
      configInput[0] = '\0';
      switch (selectedConfigOption) {
        case 1: configEditValue = getCurrentStripTypeIndex(); break;
        case 3: configEditValue = getCurrentOutputLength(); break;
        case 4: configEditValue = bus ? bus->getColorOrder() & 0x0F : COL_ORDER_GRB; break;
        case 5: configEditValue = bus && bus->isReversed(); break;
        case 6: configEditValue = bus ? bus->skippedLeds() : 0; break;
        case 7: configEditValue = bus ? bus->getLEDCurrent() : LED_MILLIAMPS_DEFAULT; break;
        case 8: configEditValue = bus ? bus->getMaxCurrent() : 0; break;
        case 9: configEditValue = BusManager::ablMilliampsMax(); break;
        case 10: configEditValue = bus ? bus->getAutoWhiteMode() : RGBW_MODE_MANUAL_ONLY; break;
      }
      updateConfigMenu();
    }

    void changeConfigValue(int8_t direction) {
      switch (selectedConfigOption) {
        case 1: {
          uint8_t count = sizeof(CARDPUTER_STRIP_TYPES) / sizeof(CARDPUTER_STRIP_TYPES[0]);
          configEditValue = (configEditValue + count + direction) % count;
          break;
        }
        case 4:
          configEditValue = (configEditValue + 6 + direction) % 6;
          break;
        case 5:
          configEditValue = !configEditValue;
          break;
        case 6:
        case 7:
        case 8:
        case 9: {
          uint16_t maximum = selectedConfigOption == 6 ? 255 : selectedConfigOption == 7 ? 255 : 65000;
          uint16_t step = selectedConfigOption == 7 ? 10 : (selectedConfigOption == 8 || selectedConfigOption == 9) ? 100 : 1;
          int32_t changed = (int32_t)configEditValue + direction * step;
          configEditValue = constrain(changed, 0, maximum);
          break;
        }
        case 10:
          configEditValue = (configEditValue + RGBW_MODE_MAX + 1 + direction) % (RGBW_MODE_MAX + 1);
          break;
      }
      configInputActive = false;
      configInputError = false;
      updateConfigMenu();
    }

    void handleConfigMenuEnter() {
      if (configEditMode) {
        bool saved = false;
        uint32_t value = configEditValue;
        bool numericOption = selectedConfigOption == 3 || selectedConfigOption == 6 ||
                             selectedConfigOption == 7 || selectedConfigOption == 8 || selectedConfigOption == 9;
        if (numericOption && configInputActive) {
          char* end = nullptr;
          value = strtoul(configInput, &end, 10);
          if (configInputLength == 0 || end == configInput || *end != '\0') {
            configInputError = true;
            updateConfigMenu();
            return;
          }
        }

        if (selectedConfigOption == 9) {
          if (value > 65000) {
            configInputError = true;
            updateConfigMenu();
            return;
          }
          BusManager::setMilliampsMax((uint16_t)value);
          BusManager::initializeABL();
          configNeedsWrite = true;
          saved = true;
        } else {
          Bus* bus = BusManager::getBus(0);
          uint8_t typeIndex = getCurrentStripTypeIndex();
          uint8_t type = bus ? bus->getType() : TYPE_WS2812_RGB;
          uint16_t ledCount = getCurrentOutputLength();
          uint8_t colorOrder = bus ? bus->getColorOrder() & 0x0F : COL_ORDER_GRB;
          bool reversed = bus && bus->isReversed();
          uint8_t skip = bus ? bus->skippedLeds() : 0;
          uint8_t whiteMode = bus ? bus->getAutoWhiteMode() : RGBW_MODE_MANUAL_ONLY;
          uint16_t ledCurrent = bus ? bus->getLEDCurrent() : LED_MILLIAMPS_DEFAULT;
          uint16_t busMaxCurrent = bus ? bus->getMaxCurrent() : 0;
          switch (selectedConfigOption) {
            case 1: typeIndex = value; type = CARDPUTER_STRIP_TYPES[typeIndex].type; break;
            case 3: ledCount = value; break;
            case 4: colorOrder = value; break;
            case 5: reversed = value != 0; break;
            case 6: skip = value; break;
            case 7: ledCurrent = value; break;
            case 8: busMaxCurrent = value; break;
            case 10: whiteMode = value; break;
          }
          if (typeIndex >= sizeof(CARDPUTER_STRIP_TYPES) / sizeof(CARDPUTER_STRIP_TYPES[0]) ||
              ledCount == 0 || ledCount > getMaxOutputLength() || colorOrder >= 6 || skip > 255 ||
              (skip > 0 && skip >= ledCount) || ledCurrent > 255 || busMaxCurrent > 65000 ||
              whiteMode > RGBW_MODE_MAX ||
              !queueOutputBusConfig(type, getCurrentOutputPin(), ledCount, colorOrder, reversed,
                                    skip, whiteMode, ledCurrent, busMaxCurrent)) {
            configInputError = true;
            updateConfigMenu();
            return;
          }
          saved = true;
        }
        if (saved) {
          configEditMode = false;
          configInputActive = false;
          configInputError = false;
        }
        updateConfigMenu();
        return;
      }

      switch (selectedConfigOption) {
        case 0:
          openShortcutConfig();
          break;
        case 2:
          openOutputPinMenu();
          break;
        case 11:
          toggleWifi();
          updateConfigMenu();
          break;
        default:
          startConfigEdit();
          break;
      }
    }

    void assignEffectShortcut(uint8_t index) {
      if (index >= 5 || selectedEffect >= strip.getModeCount()) return;
      effectShortcuts[index] = selectedEffect;
      configNeedsWrite = true;
      updateShortcutScreen();
    }

    void clearEffectShortcut(uint8_t index) {
      if (index >= 5) return;
      if (effectShortcuts[index] != UNASSIGNED_EFFECT) {
        effectShortcuts[index] = UNASSIGNED_EFFECT;
        configNeedsWrite = true;
      }
      deleteShortcutMode = false;
      updateShortcutScreen();
    }

    void activateEffectShortcut(uint8_t index) {
      uint8_t effect = effectShortcuts[index];
      if (effect == UNASSIGNED_EFFECT || effect >= strip.getModeCount()) return;

      strip.getSegment(strip.getMainSegmentId()).setMode(effect);
      setValuesFromMainSeg();
      stateUpdated(CALL_MODE_BUTTON);
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
      configNeedsWrite = true;
    }

    // Keyboard handling
    void handleKeyboard() {
      M5Cardputer.update();
      bool keyPressed = M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();

      if (keyPressed) {
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
            if (deleteShortcutMode) {
              if (key == 'c' || key == 'C') {
                deleteShortcutMode = false;
                updateShortcutScreen();
              } else if (key >= '1' && key <= '5') {
                clearEffectShortcut(key - '1');
              }
            } else if (key == 'c' || key == 'C') {
              closeShortcutConfig();
            } else if (key == '.') {
              selectedEffect = (selectedEffect + strip.getModeCount() - 1) % strip.getModeCount();
              updateShortcutScreen();
            } else if (key == ';') {
              selectedEffect = (selectedEffect + 1) % strip.getModeCount();
              updateShortcutScreen();
            } else if (key == '0') {
              deleteShortcutMode = true;
              updateShortcutScreen();
            } else if (key >= '1' && key <= '5') {
              assignEffectShortcut(key - '1');
            }
            continue;
          }

          if (configMenuMode) {
            if (configEditMode) {
              bool numericOption = selectedConfigOption == 3 || selectedConfigOption == 6 ||
                                   selectedConfigOption == 7 || selectedConfigOption == 8 || selectedConfigOption == 9;
              if (numericOption && key >= '0' && key <= '9') {
                if (configInputLength < sizeof(configInput) - 1) {
                  configInput[configInputLength++] = key;
                  configInput[configInputLength] = '\0';
                  configInputActive = true;
                  configInputError = false;
                } else {
                  configInputError = true;
                }
                updateConfigMenu();
              } else if (key == ',') {
                changeConfigValue(-1);
              } else if (key == '/') {
                changeConfigValue(1);
              } else if (key == 'c' || key == 'C') {
                configEditMode = false;
                configInputActive = false;
                configInputError = false;
                updateConfigMenu();
              }
            } else if (key == ';') {
              selectedConfigOption = (selectedConfigOption + CONFIG_OPTION_COUNT - 1) % CONFIG_OPTION_COUNT;
              updateConfigMenu();
            } else if (key == '.') {
              selectedConfigOption = (selectedConfigOption + 1) % CONFIG_OPTION_COUNT;
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
              stateUpdated(CALL_MODE_BUTTON);
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
              stateUpdated(CALL_MODE_BUTTON);
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

        bool numericOption = selectedConfigOption == 3 || selectedConfigOption == 6 ||
                             selectedConfigOption == 7 || selectedConfigOption == 8 || selectedConfigOption == 9;
        if (configMenuMode && configEditMode && numericOption && status.del) {
          if (configInputLength > 0) {
            configInput[--configInputLength] = '\0';
            configInputActive = configInputLength > 0;
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
    bool readFromConfig(JsonObject& root) override {
      JsonObject cardputerSettings = root["cardputerDisplay"];
      cardputerWifiDisabled = cardputerSettings["wifiDisabled"] | false;
      JsonArray savedEffects = cardputerSettings["effectKeys"];
      if (savedEffects.isNull() || savedEffects.size() != 5) return false;

      bool valid = true;
      for (uint8_t index = 0; index < 5; index++) {
        uint16_t savedEffect = savedEffects[index] | UNASSIGNED_EFFECT;
        if (savedEffect > 0xFF) {
          effectShortcuts[index] = UNASSIGNED_EFFECT;
          valid = false;
        } else {
          effectShortcuts[index] = (uint8_t)savedEffect;
        }
      }
      effectShortcutsLoadedFromConfig = valid;
      return valid;
    }

    void addToConfig(JsonObject& root) override {
      JsonObject cardputerSettings = root.createNestedObject("cardputerDisplay");
      cardputerSettings["wifiDisabled"] = cardputerWifiDisabled;
      JsonArray savedEffects = cardputerSettings.createNestedArray("effectKeys");
      for (uint8_t index = 0; index < 5; index++) {
        savedEffects.add(effectShortcuts[index]);
      }
    }

    void setup() override {
      // Initialize M5Cardputer with display and keyboard
      auto cfg = M5.config();
      M5Cardputer.begin(cfg, true);  // true = enable keyboard

      // Configure display
      M5Cardputer.Display.setRotation(0);  // M5GFX 0.3.x applies the Cardputer landscape offset
      M5Cardputer.Display.fillScreen(TFT_BLACK);
      M5Cardputer.Display.setBrightness(INITIAL_DISPLAY_BRIGHTNESS);

      // Initialize LVGL with M5Cardputer's display
      lvgl_setup(&M5Cardputer.Display);

      // Initialize UI (creates screens and loads main screen)
      ui_init();
      createBatteryIndicator();
      createHelpScreen();
      createShortcutScreen();
      createOutputPinScreen();
      createConfigMenuScreen();

      if (!effectShortcutsLoadedFromConfig) {
        Preferences legacyPreferences;
        if (legacyPreferences.begin("cardkeys", true)) {
          for (uint8_t index = 0; index < 5; index++) {
            char keyName[5];
            snprintf(keyName, sizeof(keyName), "key%u", index + 1);
            effectShortcuts[index] = legacyPreferences.getUChar(keyName, UNASSIGNED_EFFECT);
          }
          legacyPreferences.end();
        }
        configNeedsWrite = true;
      }
      updateShortcutScreen();
      updateConfigMenu();
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
      snprintf(heapStr, sizeof(heapStr), "%.1fkb", freeHeap / 1024.0f);
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
