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

  // Low-brightness kickstart: briefly drive the LEDs at this level when the
  // requested brightness is lower, then dim down to the requested value.
  static constexpr float MIN_STARTUP_BRIGHTNESS = 0.25f;  // 25%
  static constexpr uint32_t STARTUP_DELAY_MS = 200;

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
    ESP_LOGCONFIG("ColorTempRGBWLight", "  Low brightness startup: kickstart at 25%% for %ums",
                  (unsigned) STARTUP_DELAY_MS);
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

    // Effective brightness: include color_brightness so the white channel
    // follows the slider no matter which field Home Assistant updates.
    float raw_eff = state->current_values.get_state() * state->current_values.get_brightness() *
                    state->current_values.get_color_brightness();
    raw_eff = clamp(raw_eff, 0.0f, 1.0f);
    float brightness = state->gamma_correct_lut(raw_eff);
    bool is_on = state->current_values.is_on();

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

    ESP_LOGD(TAG, "write_state: state=%.0f brightness=%.3f color_brightness=%.3f raw_eff=%.3f gamma_eff=%.3f "
                  "mode=%d turned_on=%d",
             state->current_values.get_state(), state->current_values.get_brightness(),
             state->current_values.get_color_brightness(), raw_eff, brightness,
             (int) state->current_values.get_color_mode(), (int) turned_on);

    // Compare the raw (linear) brightness against the kickstart level so the
    // 25% threshold matches the min_power scale of the PWM outputs.
    if (turned_on && raw_eff < MIN_STARTUP_BRIGHTNESS) {
      // Turning on below the minimum reliable level: drive the LEDs at 25%
      // first so they actually ignite, then dim in loop().
      // raw_eff may be exactly 0 on the first frame of a turn-on transition.
      startup_pending_ = true;
      startup_timer_ = millis();
      apply_channels_(state, MIN_STARTUP_BRIGHTNESS, true);
      return;
    }

    startup_pending_ = false;
    apply_channels_(state, brightness, false);
  }

  void loop() override {
    if (!startup_pending_)
      return;
    if (millis() - startup_timer_ < STARTUP_DELAY_MS)
      return;

    startup_pending_ = false;
    if (light_state_ == nullptr || !light_state_->current_values.is_on())
      return;

    float raw_eff = light_state_->current_values.get_state() * light_state_->current_values.get_brightness() *
                    light_state_->current_values.get_color_brightness();
    raw_eff = clamp(raw_eff, 0.0f, 1.0f);
    apply_channels_(light_state_, light_state_->gamma_correct_lut(raw_eff), false);
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

  void apply_channels_(light::LightState *state, float brightness, bool kickstart) {
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
      // White balance (CT) with 3000K white at max, RGB assists
      float mired = state->current_values.get_color_temperature();
      mired = clamp(mired, min_mired_, max_mired_);

      // Use the brightness we were asked to write (kickstart aware), not the
      // value stored in the light state.
      float br = clamp(brightness, 0.0f, 1.0f);

      // The RGB path applies brightness twice: gamma is applied to the color
      // (which already contains brightness) and the result is then multiplied
      // by the gamma-corrected brightness again. Apply the same curve to the
      // white/tint channels so the white LED dims just as visibly as RGB.
      // Kickstart uses a flat 25% level so the LEDs can ignite.
      float level = kickstart ? br : (br * br);

      // White follows the main brightness level
      float w = level;

      // Compare target CT to native white (3000K ~= 333 mired)
      const float WHITE_K = 3000.0f;
      const float WHITE_MIRED = 1e6f / WHITE_K;  // ~333
      float xw = clamp((WHITE_MIRED - min_mired_) / (max_mired_ - min_mired_), 0.0f, 1.0f);
      float x  = clamp((mired       - min_mired_) / (max_mired_ - min_mired_), 0.0f, 1.0f);

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

      const float COOL_GAIN      = 1.00f; // overall cool tint
      const float COOL_CYAN_BIAS = 0.70f; // 0..1: portion of cool tint sent to GREEN
      const float ANTI_MAGENTA   = 0.20f; // extra green as it gets cooler

      float r = level * (WARM_R_GAIN * warm_strength);
      float g = level * (WARM_G_GAIN * warm_strength
                    + COOL_GAIN * cool_strength * (COOL_CYAN_BIAS * (1.0f + ANTI_MAGENTA * cool_strength)));
      float b = level * (COOL_GAIN * cool_strength * (1.0f - COOL_CYAN_BIAS));

      r = clamp(r, 0.0f, 1.0f);
      g = clamp(g, 0.0f, 1.0f);
      b = clamp(b, 0.0f, 1.0f);
      w = clamp(w, 0.0f, 1.0f);

      ESP_LOGD(TAG, "CT mode: br=%.3f level=%.3f kickstart=%d -> R=%.3f G=%.3f B=%.3f W=%.3f", br, level,
               (int) kickstart, r, g, b, w);

      if (red_)   red_->set_level(r);
      if (green_) green_->set_level(g);
      if (blue_)  blue_->set_level(b);
      if (white_) white_->set_level(w);

      color_temp_ = mired;
      brightness_ = br;
      return;
    }

    // --- Color wheel mode (RGB/HS) ---
    float r = 0, g = 0, b = 0, cwhite = 0, wwhite = 0;
    // This gives us normalized RGB already brightness-scaled by HA
    state->current_values_as_rgbww(&r, &g, &b, &cwhite, &wwhite);

    // Enforce our own brightness scaling, keeps behavior consistent between modes
    float factor = brightness;
    if (kickstart) {
      // Scale the color up so the brightest channel reaches the kickstart level
      float max_channel = std::max(r, std::max(g, b));
      factor = (max_channel > 0.0001f) ? (MIN_STARTUP_BRIGHTNESS / max_channel) : 1.0f;
    }
    r = clamp(r * factor, 0.0f, 1.0f);
    g = clamp(g * factor, 0.0f, 1.0f);
    b = clamp(b * factor, 0.0f, 1.0f);

    ESP_LOGD(TAG, "RGB mode: factor=%.3f kickstart=%d -> R=%.3f G=%.3f B=%.3f", factor, (int) kickstart, r, g, b);

    if (red_)   red_->set_level(r);
    if (green_) green_->set_level(g);
    if (blue_)  blue_->set_level(b);

    // In RGB mode the dedicated white channel is off (Hue-like behavior)
    if (white_) white_->set_level(0.0f);

    brightness_ = brightness;
  }

  static float clamp(float x, float min_val, float max_val) {
    return std::max(std::min(x, max_val), min_val);
  }
};

}  // namespace color_temp_rgbw_light
}  // namespace esphome
