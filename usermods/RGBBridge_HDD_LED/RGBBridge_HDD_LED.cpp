// RGBBridge HDD LED v0.2 - WLED main / ESP32 (Arduino-ESP32 3.x)
// Separate physical input and physical PWM output. No WLED LED segment is changed.
#include "wled.h"
#include "HddLedLogic.h"

#ifndef ARDUINO_ARCH_ESP32
  #error RGBBridge HDD LED requires an ESP32 and a hardware LEDC PWM channel.
#endif

namespace {
  // Private owner/ID in the usermod ID range (< 0x80). If another local
  // usermod already uses 0x7E, assign a different unused value in BOTH places.
  constexpr uint16_t HDD_USERMOD_ID = 0x7E;
  constexpr PinOwner HDD_OWNER = static_cast<PinOwner>(HDD_USERMOD_ID);
  constexpr uint32_t HDD_PWM_HZ = 5000;
  constexpr uint8_t HDD_PWM_BITS = 8;
  volatile uint32_t hddEdges = 0;
  void IRAM_ATTR onHddActiveEdge() { ++hddEdges; }
}

class RGBBridgeHddLed : public Usermod {
private:
  static const char _name[];
  bool enabled_ = false; // pin-safe: must be enabled explicitly after wiring
  int8_t inputPin_ = -1;
  int8_t outputPin_ = -1;
  bool activeLow_ = true; // PC817 collector + pull-up: LOW = HDD active
  HddLedSettings settings_{};
  HddLedLogic logic_{};

  bool started_ = false;
  bool inputReady_ = false;
  bool outputReady_ = false;
  int8_t assignedInput_ = -1;
  int8_t assignedOutput_ = -1;
  uint8_t ledcChannel_ = 255;

  void writePwm(uint8_t duty) {
    if (!outputReady_) return;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    ledcWrite(ledcChannel_, duty); // Arduino-ESP32 2.x: channel argument
#else
    ledcWrite(static_cast<uint8_t>(assignedOutput_), duty); // Arduino-ESP32 3.x: pin argument
#endif
  }

  void releaseHardware() {
    if (assignedInput_ >= 0) {
      detachInterrupt(digitalPinToInterrupt(assignedInput_));
      PinManager::deallocatePin(static_cast<uint8_t>(assignedInput_), HDD_OWNER);
      assignedInput_ = -1;
    }
    inputReady_ = false;
    if (assignedOutput_ >= 0) {
      if (outputReady_) writePwm(0);
      if (ledcChannel_ != 255) {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        ledcDetachPin(static_cast<uint8_t>(assignedOutput_));
#else
        ledcDetach(static_cast<uint8_t>(assignedOutput_));
#endif
        PinManager::deallocateLedc(ledcChannel_, 1);
        ledcChannel_ = 255;
      }
      pinMode(static_cast<uint8_t>(assignedOutput_), INPUT);
      PinManager::deallocatePin(static_cast<uint8_t>(assignedOutput_), HDD_OWNER);
      assignedOutput_ = -1;
    }
    outputReady_ = false;
  }

  void reconfigureHardware() {
    releaseHardware();
    logic_.reset(millis());
    noInterrupts(); hddEdges = 0; interrupts();
    if (!enabled_ || outputPin_ < 0 || inputPin_ < 0) return;
    if (inputPin_ == outputPin_) return;
    if (!PinManager::isPinOk(static_cast<uint8_t>(outputPin_), true)) return;
    if (!PinManager::isPinOk(static_cast<uint8_t>(inputPin_), false)) return;
    if (!PinManager::allocatePin(static_cast<uint8_t>(inputPin_), false, HDD_OWNER)) return;
    assignedInput_ = inputPin_;
    // ESP32 GPIO34..39 require EXTERNAL pull-up; INPUT_PULLUP does not add one.
    pinMode(static_cast<uint8_t>(assignedInput_), INPUT_PULLUP);
    if (!PinManager::allocatePin(static_cast<uint8_t>(outputPin_), true, HDD_OWNER)) {
      releaseHardware(); return;
    }
    assignedOutput_ = outputPin_;
    pinMode(static_cast<uint8_t>(assignedOutput_), OUTPUT);
    digitalWrite(static_cast<uint8_t>(assignedOutput_), LOW);
    ledcChannel_ = PinManager::allocateLedc(1);
    if (ledcChannel_ == 255) { releaseHardware(); return; }
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    if (ledcSetup(ledcChannel_, HDD_PWM_HZ, HDD_PWM_BITS) <= 0) {
      PinManager::deallocateLedc(ledcChannel_, 1);
      ledcChannel_ = 255; releaseHardware(); return;
    }
    ledcAttachPin(static_cast<uint8_t>(assignedOutput_), ledcChannel_);
#else
    if (!ledcAttachChannel(static_cast<uint8_t>(assignedOutput_), HDD_PWM_HZ,
                           HDD_PWM_BITS, ledcChannel_)) {
      PinManager::deallocateLedc(ledcChannel_, 1);
      ledcChannel_ = 255; releaseHardware(); return;
    }
#endif
    outputReady_ = true;
    writePwm(0);
    if (assignedInput_ >= 0) {
      noInterrupts(); hddEdges = 0; interrupts();
      attachInterrupt(digitalPinToInterrupt(assignedInput_), onHddActiveEdge,
                      activeLow_ ? FALLING : RISING);
      inputReady_ = true;
    }
  }

public:
  void setup() override {
    started_ = true;
    reconfigureHardware();
  }

  void loop() override {
    if (!enabled_ || !outputReady_) return;
    bool active = false;
    bool edge = false;
    if (inputReady_) {
      active = digitalRead(static_cast<uint8_t>(assignedInput_)) ==
               (activeLow_ ? LOW : HIGH);
      noInterrupts();
      edge = hddEdges != 0;
      hddEdges = 0;
      interrupts();
    }
    writePwm(logic_.tick(millis(), active, edge, settings_));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject cfg = root.createNestedObject(_name);
    cfg["enabled"] = enabled_;
    JsonArray pins = cfg.createNestedArray("pin");
    pins.add(inputPin_); pins.add(outputPin_);
    cfg["activeLow"] = activeLow_;
    cfg["mode"] = settings_.mode; // 0 direct / 1 fade / 2 blink
    cfg["fadeInMs"] = settings_.fadeInMs;
    cfg["fadeOutMs"] = settings_.fadeOutMs;
    cfg["pulseHoldMs"] = settings_.pulseHoldMs;
    cfg["blinkWindowMs"] = settings_.blinkWindowMs;
    cfg["blinkPeakMs"] = settings_.blinkPeakMs;
    cfg["blinkPauseMs"] = settings_.blinkPauseMs;
    cfg["maxBrightness"] = settings_.maxBrightness;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject cfg = root[_name];
    if (cfg.isNull()) return false; // WLED stores defaults during boot
    bool complete = true;
    complete &= getJsonValue(cfg["enabled"], enabled_, false);
    complete &= getJsonValue(cfg["pin"][0], inputPin_, -1);
    complete &= getJsonValue(cfg["pin"][1], outputPin_, -1);
    complete &= getJsonValue(cfg["activeLow"], activeLow_, true);
    complete &= getJsonValue(cfg["mode"], settings_.mode, uint8_t(1));
    complete &= getJsonValue(cfg["fadeInMs"], settings_.fadeInMs, uint16_t(30));
    complete &= getJsonValue(cfg["fadeOutMs"], settings_.fadeOutMs, uint16_t(180));
    complete &= getJsonValue(cfg["pulseHoldMs"], settings_.pulseHoldMs, uint16_t(60));
    complete &= getJsonValue(cfg["blinkWindowMs"], settings_.blinkWindowMs, uint16_t(500));
    complete &= getJsonValue(cfg["blinkPeakMs"], settings_.blinkPeakMs, uint16_t(25));
    complete &= getJsonValue(cfg["blinkPauseMs"], settings_.blinkPauseMs, uint16_t(70));
    complete &= getJsonValue(cfg["maxBrightness"], settings_.maxBrightness, uint8_t(255));
    if (settings_.mode > 2) settings_.mode = 1;
    // Called before setup on boot, and again after a Usermod Settings save.
    if (started_) reconfigureHardware();
    return complete;
  }

  void appendConfigData() override {
    // Dropdowns for the numeric settings on the standard WLED Usermod Settings page.
    oappend(F("hd=addDropdown('RGBBridge-HDD','mode');"));
    oappend(F("addOption(hd,'Direct',0);addOption(hd,'Fade',1);addOption(hd,'Blink + Fade',2);"));
    oappend(F("addInfo('RGBBridge-HDD:pin[]',0,'','HDD GPIO input');"));
    oappend(F("addInfo('RGBBridge-HDD:pin[]',1,'','PWM GPIO output');"));
    oappend(F("addInfo('RGBBridge-HDD:fadeInMs',1,'ms');"));
    oappend(F("addInfo('RGBBridge-HDD:fadeOutMs',1,'ms');"));
    oappend(F("addInfo('RGBBridge-HDD:pulseHoldMs',1,'ms (Fade only)');"));
    oappend(F("addInfo('RGBBridge-HDD:blinkWindowMs',1,'ms (Blink activity window)');"));
    oappend(F("addInfo('RGBBridge-HDD:blinkPeakMs',1,'ms');"));
    oappend(F("addInfo('RGBBridge-HDD:blinkPauseMs',1,'ms');"));
    oappend(F("addInfo('RGBBridge-HDD:maxBrightness',1,'0-255');"));
  }

  uint16_t getId() override { return HDD_USERMOD_ID; }
};

const char RGBBridgeHddLed::_name[] PROGMEM = "RGBBridge-HDD";
static RGBBridgeHddLed rgbBridgeHddLed;
REGISTER_USERMOD(rgbBridgeHddLed);
