#pragma once
#include <stdint.h>

// Pure, hardware-independent PWM brightness envelope (all times in ms).
// mode: 0 = direct HDD signal, 1 = fade following HDD,
//       2 = independent blink rhythm gated by a retriggerable HDD activity window.
struct HddLedSettings {
  uint8_t mode = 1;
  uint16_t fadeInMs = 30;
  uint16_t fadeOutMs = 180;
  uint16_t pulseHoldMs = 60;    // Fade mode: stretches very short HDD pulses.
  uint16_t blinkWindowMs = 500; // Blink mode: resets on every HDD pulse.
  uint16_t blinkPeakMs = 25;    // Blink mode: time at full brightness.
  uint16_t blinkPauseMs = 70;   // Blink mode: darkness between cycles.
  uint8_t maxBrightness = 255;
};

class HddLedLogic {
  float level_ = 0.0f;  // normalized brightness, 0..255
  uint32_t lastTickMs_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t blinkStartMs_ = 0;
  uint32_t stopStartMs_ = 0;
  float blinkStartLevel_ = 0.0f; // prevent a jump on a new pulse during shutdown
  float stopStartLevel_ = 0.0f;  // fade smoothly from last rendered brightness
  bool initialized_ = false;
  bool seenActivity_ = false;
  bool blinking_ = false;
  bool stopping_ = false;

  static float clamp255(float value) {
    return value < 0.0f ? 0.0f : (value > 255.0f ? 255.0f : value);
  }

  static float advance(float value, float target, uint32_t deltaMs, uint16_t rampMs) {
    if (!rampMs) return target;
    const float step = 255.0f * float(deltaMs) / float(rampMs);
    if (value < target) return clamp255(value + step > target ? target : value + step);
    return clamp255(value - step < target ? target : value - step);
  }

  float blinkLevel(uint32_t nowMs, const HddLedSettings &s) const {
    uint32_t period = uint32_t(s.fadeInMs) + s.blinkPeakMs +
                      s.fadeOutMs + s.blinkPauseMs;
    if (!period) period = 1; // all zero: don't divide by zero
    uint32_t phase = uint32_t(nowMs - blinkStartMs_) % period;
    if (phase < s.fadeInMs) {
      const float progress = float(phase) / float(s.fadeInMs);
      return blinkStartLevel_ + (255.0f - blinkStartLevel_) * progress;
    }
    phase -= s.fadeInMs;
    if (phase < s.blinkPeakMs) return 255.0f;
    phase -= s.blinkPeakMs;
    if (phase < s.fadeOutMs) {
      return 255.0f * (1.0f - float(phase) / float(s.fadeOutMs));
    }
    return 0.0f;
  }

public:
  void reset(uint32_t nowMs = 0) {
    level_ = 0.0f;
    lastTickMs_ = nowMs;
    lastActivityMs_ = nowMs;
    blinkStartMs_ = nowMs;
    stopStartMs_ = nowMs;
    blinkStartLevel_ = 0.0f;
    stopStartLevel_ = 0.0f;
    initialized_ = false;
    seenActivity_ = false;
    blinking_ = false;
    stopping_ = false;
  }

  uint8_t tick(uint32_t nowMs, bool inputActive, bool activityEdge,
               const HddLedSettings &s) {
    if (!initialized_) { lastTickMs_ = nowMs; initialized_ = true; }
    const uint32_t deltaMs = nowMs - lastTickMs_; // handles millis() rollover
    lastTickMs_ = nowMs;

    if (inputActive || activityEdge) {
      lastActivityMs_ = nowMs; // every new pulse extends the blink window
      seenActivity_ = true;
    }

    if (s.mode == 0) { // direct HDD signal (captured short edges get one tick)
      blinking_ = false;
      stopping_ = false;
      level_ = (inputActive || activityEdge) ? 255.0f : 0.0f;
    } else if (s.mode == 1) { // follow HDD, with optional pulse stretch
      blinking_ = false;
      stopping_ = false;
      const bool recent = inputActive ||
          (seenActivity_ && uint32_t(nowMs - lastActivityMs_) < s.pulseHoldMs);
      level_ = advance(level_, recent ? 255.0f : 0.0f, deltaMs,
                       recent ? s.fadeInMs : s.fadeOutMs);
    } else { // blink + fade: activity enables a fixed independent rhythm
      const bool inWindow = inputActive || activityEdge ||
          (seenActivity_ && uint32_t(nowMs - lastActivityMs_) < s.blinkWindowMs);

      if (inWindow) {
        if (!blinking_) {
          blinking_ = true;
          stopping_ = false;
          blinkStartMs_ = nowMs;
          blinkStartLevel_ = level_;
        }
        level_ = blinkLevel(nowMs, s);
      } else {
        if (blinking_) { // activity window just expired: begin final fade out
          blinking_ = false;
          stopping_ = true;
          stopStartMs_ = nowMs;
          stopStartLevel_ = level_;
        }
        if (stopping_ && s.fadeOutMs != 0) {
          const uint32_t stopElapsed = uint32_t(nowMs - stopStartMs_);
          if (stopElapsed < s.fadeOutMs) {
            level_ = stopStartLevel_ *
                     (1.0f - float(stopElapsed) / float(s.fadeOutMs));
          } else {
            level_ = 0.0f;
            stopping_ = false;
          }
        } else {
          level_ = 0.0f;
          stopping_ = false;
        }
      }
    }
    return static_cast<uint8_t>(
        clamp255(level_) * float(s.maxBrightness) / 255.0f + 0.5f);
  }
};
