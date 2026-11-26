#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace color_temp_rgbw_light {

static const char *const TAG = "color_temp_rgbw";

class ColorTempRGBWLight : public Component, public light::LightOutput {
 public:
  // Default min/max mireds (shamelessly stolen from Philips Hue)
  static constexpr float max_mired_ = 500.0f;  // ~2000K
  static constexpr float min_mired_ = 153.0f;  // ~6500K

  // Low-brightness kickstart: on an off -> on transition below
  // STARTUP_THRESHOLD (perceptual slider position) the LEDs are briefly driven
  // at STARTUP_LEVEL so the driver ignites, then they dim to the target level.
  static constexpr float STARTUP_THRESHOLD = 0.25f;  // 25% of the HA slider
  static constexpr float STARTUP_LEVEL = 0.30f;      // PWM level (~25% duty with max_power 80%)
  static constexpr uint32_t STARTUP_DELAY_MS = 200;

  // output_white uses max_power: 80% in the YAML while the RGB outputs use 100%.
  // That only compresses the white channel's absolute PWM duty by a constant
  // factor; since the colour tints scale linearly with the same level as white,
  // the white/tint ratio -- and therefore the perceived colour temperature --
  // stays the same at every brightness. (The old min_power floor broke exactly
  // that: it clamped every channel to the same absolute duty at low levels, so
  // the warm/cool tint lost its relative strength and the bulb drifted cooler as
  // it dimmed.)

  ColorTempRGBWLight()
      : red_(nullptr), green_(nullptr), blue_(nullptr), white_(nullptr),
        color_temp_(-1.0f), brightness_(-1.0f), last_mode_(light::ColorMode::UNKNOWN),
        light_state_(nullptr), startup_timer_(0), startup_pending_(false), was_on_(false),
        // linear CT -> channel coefficients (warmer = redder, not yellow)
        br_(0.00f), cr_(1.25f),
        bg_(-0.05f), cg_(0.20f),
        bb_(0.60f), cb_(-1.00f),
        bw_(1.00f), cw_(-0.90f) {}

  // setters for outputs
  void set_red(output::FloatOutput *red) { red_ = red; }
  void set_green(output::FloatOutput *green) { green_ = green; }
  void set_blue(output::FloatOutput *blue) { blue_ = blue; }
  void set_white(output::FloatOutput *white) { white_ = white; }

  void setup() override {}

  void dump_config() override {
    ESP_LOGCONFIG("ColorTempRGBWLight", "ColorTempRGBWLight:");
    if (red_ != nullptr) ESP_LOGCONFIG("ColorTempRGBWLight", "  Red output attached");
    if (green_ != nullptr) ESP_LOGCONFIG("ColorTempRGBWLight", "  Green output attached");
    if (blue_ != nullptr) ESP_LOGCONFIG("ColorTempRGBWLight", "  Blue output attached");
    if (white_ != nullptr) ESP_LOGCONFIG("ColorTempRGBWLight", "  White output attached");
    ESP_LOGCONFIG("ColorTempRGBWLight", "  Modes: RGB (color wheel) + COLOR_TEMPERATURE (white balance)");
    ESP_LOGCONFIG("ColorTempRGBWLight", "  Low brightness startup: kickstart at level %.2f for %ums",
                  STARTUP_LEVEL, (unsigned) STARTUP_DELAY_MS);
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(min_mired_);
    traits.set_max_mireds(max_mired_);
    return traits;
  }

  void write_state(light::LightState *state) override {
    light_state_ = state;
    const light::LightColorValues &values = state->current_values;

    // Perceptual (0..1) brightness as reported by Home Assistant.
    float raw = clamp(values.get_state() * values.get_brightness() * values.get_color_brightness(), 0.0f, 1.0f);
    // Gamma is applied exactly ONCE. This is ESPHome's standard mapping from the
    // brightness slider to the linear PWM duty cycle; applying it a second time
    // pushes almost the whole slider range onto the outputs' minimum power.
    float level = state->gamma_correct_lut(raw);
    bool is_on = values.is_on();

    if (!is_on) {
      startup_pending_ = false;
      was_on_ = false;
      apply_channels_(state, 0.0f, false);
      return;
    }

    // Kickstart only on an off -> on transition. Brightness changes while the
    // bulb is already on are applied directly, without the high-power blip.
    bool turned_on = !was_on_;
    was_on_ = true;

    ESP_LOGD(TAG, "write_state: state=%.0f brightness=%.3f color_brightness=%.3f raw=%.3f level=%.3f "
                  "mode=%d turned_on=%d",
             values.get_state(), values.get_brightness(), values.get_color_brightness(), raw, level,
             (int) values.get_color_mode(), (int) turned_on);

    if (turned_on && raw < STARTUP_THRESHOLD) {
      // Turning on below the minimum reliable level: drive the LEDs at the
      // startup level first so they actually ignite, then dim in loop().
      // raw may be exactly 0 on the first frame of a turn-on transition.
      startup_pending_ = true;
      startup_timer_ = millis();
      apply_channels_(state, STARTUP_LEVEL, true);
      return;
    }

    startup_pending_ = false;
    apply_channels_(state, level, false);
  }

  void loop() override {
    if (!startup_pending_)
      return;
    if (millis() - startup_timer_ < STARTUP_DELAY_MS)
      return;

    startup_pending_ = false;
    if (light_state_ == nullptr || !light_state_->current_values.is_on())
      return;

    const light::LightColorValues &values = light_state_->current_values;
    float raw = clamp(values.get_state() * values.get_brightness() * values.get_color_brightness(), 0.0f, 1.0f);
    apply_channels_(light_state_, light_state_->gamma_correct_lut(raw), false);
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  output::FloatOutput *white_;

  float color_temp_;
  float brightness_;
  light::ColorMode last_mode_;

  // Low brightness kickstart state
  light::LightState *light_state_;
  uint32_t startup_timer_;
  bool startup_pending_;
  bool was_on_;

  // Linear CT mix params
  float br_, cr_;
  float bg_, cg_;
  float bb_, cb_;
  float bw_, cw_;

  void apply_channels_(light::LightState *state, float level, bool kickstart) {
    if (!state->current_values.is_on()) {
      if (red_ != nullptr) red_->set_level(0.0f);
      if (green_ != nullptr) green_->set_level(0.0f);
      if (blue_ != nullptr) blue_->set_level(0.0f);
      if (white_ != nullptr) white_->set_level(0.0f);
      brightness_ = 0.0f;
      return;
    }

    // What mode is the user currently in?
    light::ColorMode mode = state->current_values.get_color_mode();
    last_mode_ = mode;

    if (mode == light::ColorMode::COLOR_TEMPERATURE) {
      // White balance (CT) with 3000K white at max, RGB assists.
      float mired = state->current_values.get_color_temperature();
      mired = clamp(mired, min_mired_, max_mired_);

      // Linear PWM level for the current brightness. Gamma was applied exactly
      // once in write_state()/loop(), just like the RGB path below, so both
      // modes dim identically over the whole slider travel.
      float lvl = clamp(level, 0.0f, 1.0f);

      // White follows the main brightness level. The colour tints are derived
      // from the same level (white is only compressed by output_white's
      // max_power: 80%, a constant factor), so the white/tint ratio -- and thus
      // the perceived colour temperature -- is identical at every brightness.
      float w = lvl;
      float tint = lvl;

      // Compare target CT to native white (3000K ~= 333 mired)
      const float WHITE_K = 3000.0f;
      const float WHITE_MIRED = 1e6f / WHITE_K;  // ~333
      float xw = clamp((WHITE_MIRED - min_mired_) / (max_mired_ - min_mired_), 0.0f, 1.0f);
      float x = clamp((mired - min_mired_) / (max_mired_ - min_mired_), 0.0f, 1.0f);

      float delta = x - xw;
      float warm_strength = (delta > 0.0f) ? (delta / (1.0f - xw)) : 0.0f;
      float cool_strength = (delta < 0.0f) ? (-delta / (xw > 0 ? xw : 1.0f)) : 0.0f;

      // Gentle curve so small moves don't over-tint
      const float curve = 1.15f;
      warm_strength = std::pow(warm_strength, curve);
      cool_strength = std::pow(cool_strength, curve);

      // Tuning: warm = red (+ tiny green), cool = cyan (more green than blue)
      const float WARM_R_GAIN = 1.00f;
      const float WARM_G_GAIN = 0.10f;

      const float COOL_GAIN = 1.00f;       // overall cool tint
      const float COOL_CYAN_BIAS = 0.70f;  // 0..1: portion of cool tint sent to GREEN
      const float ANTI_MAGENTA = 0.20f;    // extra green as it gets cooler

      float r = tint * (WARM_R_GAIN * warm_strength);
      float g = tint * (WARM_G_GAIN * warm_strength +
                        COOL_GAIN * cool_strength * (COOL_CYAN_BIAS * (1.0f + ANTI_MAGENTA * cool_strength)));
      float b = tint * (COOL_GAIN * cool_strength * (1.0f - COOL_CYAN_BIAS));

      r = clamp(r, 0.0f, 1.0f);
      g = clamp(g, 0.0f, 1.0f);
      b = clamp(b, 0.0f, 1.0f);
      w = clamp(w, 0.0f, 1.0f);

      ESP_LOGD(TAG, "CT mode: level=%.3f warm=%.3f cool=%.3f kickstart=%d -> R=%.3f G=%.3f B=%.3f W=%.3f", lvl,
               warm_strength, cool_strength, (int) kickstart, r, g, b, w);

      if (red_) red_->set_level(r);
      if (green_) green_->set_level(g);
      if (blue_) blue_->set_level(b);
      if (white_) white_->set_level(w);

      color_temp_ = mired;
      brightness_ = lvl;
      return;
    }

    // --- Color wheel mode (RGB/HS) ---
    // current_values_as_rgb() applies gamma exactly once (same as the CT branch
    // above), so the perceived brightness matches between the two modes.
    float r = 0, g = 0, b = 0;
    state->current_values_as_rgb(&r, &g, &b);

    if (kickstart) {
      // Scale the color up so the brightest channel reaches the startup level
      float max_channel = std::max(r, std::max(g, b));
      float factor = (max_channel > 0.0001f) ? (STARTUP_LEVEL / max_channel) : 1.0f;
      r *= factor;
      g *= factor;
      b *= factor;
    }

    r = clamp(r, 0.0f, 1.0f);
    g = clamp(g, 0.0f, 1.0f);
    b = clamp(b, 0.0f, 1.0f);

    ESP_LOGD(TAG, "RGB mode: kickstart=%d -> R=%.3f G=%.3f B=%.3f", (int) kickstart, r, g, b);

    if (red_) red_->set_level(r);
    if (green_) green_->set_level(g);
    if (blue_) blue_->set_level(b);

    // In RGB mode the dedicated white channel is off (Hue-like behavior)
    if (white_) white_->set_level(0.0f);

    brightness_ = level;
  }

  static float clamp(float x, float min_val, float max_val) {
    return std::max(std::min(x, max_val), min_val);
  }
};

}  // namespace color_temp_rgbw_light
}  // namespace esphome
