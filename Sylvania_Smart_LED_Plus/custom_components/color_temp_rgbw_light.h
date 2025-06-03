#pragma once
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace color_temp_rgbw_light {

class ColorTempRGBWLight : public Component, public light::LightOutput {
 public:
  // Default min/max mireds
  static constexpr float max_mired_ = 500.0f;
  static constexpr float min_mired_ = 153.0f;

  // Default constructor with hardcoded scaling
  ColorTempRGBWLight()
      : red_(nullptr), green_(nullptr), blue_(nullptr), white_(nullptr),
        color_temp_(-1.0f), brightness_(-1.0f), rgb_changed_(false),
        br_(0.0f), cr_(1.0f), bg_(0.0f), cg_(0.6f), bb_(0.5f), cb_(-1.5f), bw_(1.0f), cw_(-0.7f) {}

  // Optional setters for outputs
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
  }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB_COLOR_TEMPERATURE});
    traits.set_min_mireds(min_mired_);
    traits.set_max_mireds(max_mired_);
    return traits;
  }

  void write_state(light::LightState *state) override {
    float brightness;
    state->current_values_as_brightness(&brightness);
    float color_temp = state->current_values.get_color_temperature();

    if (color_temp != color_temp_ || (!rgb_changed_ && brightness != brightness_)) {
      float xaxis = (color_temp - min_mired_) / (max_mired_ - min_mired_);
      float red = clamp((cr_ * xaxis + br_) * brightness, 0.0f, 1.0f);
      float green = clamp((cg_ * xaxis + bg_) * brightness, 0.0f, 1.0f);
      float blue = clamp((cb_ * xaxis + bb_) * brightness, 0.0f, 1.0f);
      float white = clamp((cw_ * xaxis + bw_) * brightness, 0.0f, 1.0f);

      if (red_) red_->set_level(red);
      if (green_) green_->set_level(green);
      if (blue_) blue_->set_level(blue);
      if (white_) white_->set_level(white);

      color_temp_ = color_temp;
      rgb_changed_ = false;
    } else {
      float red, green, blue, cwhite, wwhite;
      state->current_values_as_rgbww(&red, &green, &blue, &cwhite, &wwhite);

      if (red_) red_->set_level(red);
      if (green_) green_->set_level(green);
      if (blue_) blue_->set_level(blue);
      if (white_) white_->set_level(0);

      rgb_changed_ = true;
    }

    brightness_ = brightness;
  }

 protected:
  output::FloatOutput *red_;
  output::FloatOutput *green_;
  output::FloatOutput *blue_;
  output::FloatOutput *white_;

  float color_temp_;
  float brightness_;
  bool rgb_changed_;

  float br_, cr_;
  float bg_, cg_;
  float bb_, cb_;
  float bw_, cw_;

  float clamp(float x, float min_val, float max_val) {
    return std::max(std::min(x, max_val), min_val);
  }
};

}  // namespace color_temp_rgbw_light
}  // namespace esphome