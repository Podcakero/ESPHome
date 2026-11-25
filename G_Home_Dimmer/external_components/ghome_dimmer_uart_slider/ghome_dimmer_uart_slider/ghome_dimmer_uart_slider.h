#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/light/light_state.h"

namespace esphome {
namespace ghome_dimmer_uart_slider {

#define TAG "ghome_uart"

class GHomeDimmerUartSlider : public Component, public uart::UARTDevice {
 public:
  GHomeDimmerUartSlider() = default;
  
  void set_switch_light(light::LightState *light) { switch_light_ = light; }

  void setup() override {
    ESP_LOGI(TAG, "Setting up GHome Dimmer UART Slider");
    
    // Sync HA light state to dimmer on boot
    if (switch_light_ != nullptr) {
      float state = switch_light_->current_values.get_state();
      float brightness = switch_light_->current_values.get_brightness();
      if (state != 1.0f || brightness <= 0.0f) {
        uint8_t cmd = 0x7F;
        write_array(&cmd, 1);
      } else {
        int min_cmd = 0x80;
        int max_cmd = 0xB0;
        int scaled = int(brightness * (max_cmd - min_cmd) + min_cmd + 0.5f);
        if (scaled > max_cmd) scaled = max_cmd;
        uint8_t cmd = scaled;
        write_array(&cmd, 1);
      }
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "GHome Dimmer UART Slider");
  }

  void loop() override {
    // Keep reading while at least 5 bytes are available
    while (available() >= 5) {
      // Peek first byte; valid packets start with 0x24
      if (peek() == 0x24) {
        uint8_t data[5];
        read_array(data, 5);

        ESP_LOGD(TAG, "Received raw UART: %02X %02X %02X %02X %02X",
                 data[0], data[1], data[2], data[3], data[4]);

        // Valid touch packet if byte2==0x01 and byte4==0x23
        if (data[2] == 0x01 && data[4] == 0x23) {
          int raw_val = data[1];  // ranges roughly 0x01..0x96
          float brightness = raw_val / 150.0f;  // approximate 0..100%
          if (brightness > 1.0f) brightness = 1.0f;
          ESP_LOGI(TAG, "Touch Slider → raw: %d → %.1f%%", raw_val, brightness * 100.0f);
          
          if (switch_light_ != nullptr) {
            auto call = switch_light_->make_call();
            if (brightness <= 0.05f) {
              call.set_state(false);
            } else {
              call.set_brightness(brightness);
              call.set_state(true);
            }
            call.perform();
          }
        }
      } else {
        // Not a valid packet start; drop one byte and continue
        read();
      }
    }
  }

 protected:
  light::LightState *switch_light_{nullptr};
};

}  // namespace ghome_dimmer_uart_slider
}  // namespace esphome