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

class ColorTempRGBWLight : public Component, public light::LightOutput {
 public:
  // Default min/max mireds (shamelessly stolen from Philips Hue)
  static constexpr float max_mired_ = 500.0f;  // ~2000K
  static constexpr float min_mired_ = 153.0f;  // ~6500K

  ColorTempRGBWLight()
      : red_(nullptr), green_(nullptr), blue_(nullptr), white_(nullptr),
        color_temp_(-1.0f), brightness_(-1.0f), last_mode_(light::ColorMode::UNKNOWN),
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
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    // Expose **two separate modes** so HA shows a color wheel AND a CT slider with a mode toggle (this took waaaayyy too long tbh, turns out color temp is different from slider).
    traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(min_mired_);
    traits.set_max_mireds(max_mired_);
    return traits;
  }

  void write_state(light::LightState *state) override {
    float brightness = 1.0f;
    state->current_values_as_brightness(&brightness);

    // What mode is the user currently in?
    light::ColorMode mode = state->current_values.get_color_mode();
    last_mode_ = mode;

    if (mode == light::ColorMode::COLOR_TEMPERATURE) {
  // White balance (CT) with 3000K white at max, RGB assists
  float mired = state->current_values.get_color_temperature();
  mired = clamp(mired, min_mired_, max_mired_);

  float br = 1.0f;
  state->current_values_as_brightness(&br);
  br = clamp(br, 0.0f, 1.0f);

  // White stays pegged to main brightness
  float w = br;

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

  float r = br * (WARM_R_GAIN * warm_strength);
  float g = br * (WARM_G_GAIN * warm_strength
                + COOL_GAIN * cool_strength * (COOL_CYAN_BIAS * (1.0f + ANTI_MAGENTA * cool_strength)));
  float b = br * (COOL_GAIN * cool_strength * (1.0f - COOL_CYAN_BIAS));

  r = clamp(r, 0.0f, 1.0f);
  g = clamp(g, 0.0f, 1.0f);
  b = clamp(b, 0.0f, 1.0f);
  w = clamp(w, 0.0f, 1.0f);

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
    r = clamp(r * brightness, 0.0f, 1.0f);
    g = clamp(g * brightness, 0.0f, 1.0f);
    b = clamp(b * brightness, 0.0f, 1.0f);

    if (red_)   red_->set_level(r);
    if (green_) green_->set_level(g);
    if (blue_)  blue_->set_level(b);

    // In RGB mode the dedicated white channel is off (Hue-like behavior)
    if (white_) white_->set_level(0.0f);

    brightness_ = brightness;
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  output::FloatOutput *white_;

  float color_temp_;
  float brightness_;
  light::ColorMode last_mode_;

  // Linear CT mix params
  float br_, cr_;
  float bg_, cg_;
  float bb_, cb_;
  float bw_, cw_;

  static float clamp(float x, float min_val, float max_val) {
    return std::max(std::min(x, max_val), min_val);
  }
};

}  // namespace color_temp_rgbw_light
}  // namespace esphome