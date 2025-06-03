#include "esphome.h"

class GHomeDimmerUartSlider : public Component, public UARTDevice {
 public:
  GHomeDimmerUartSlider(UARTComponent *parent, sensor::Sensor *sensor)
      : UARTDevice(parent), slider_sensor(sensor) {}

  void setup() override {
    // Sync HA slider state to dimmer on boot
    float value = id(dimmer_slider).state;
    if (value <= 0.0f) {
      uint8_t cmd = 0x7F;
      write_array(&cmd, 1);
    } else {
      int min_cmd = 0x80;
      int max_cmd = 0xB0;
      int scaled = int((value / 100.0f) * (max_cmd - min_cmd) + min_cmd + 0.5f);
      if (scaled > max_cmd) scaled = max_cmd;
      uint8_t cmd = scaled;
      write_array(&cmd, 1);
    }
  }

  void loop() override {
    // Keep reading while at least 5 bytes are available
    while (available() >= 5) {
      // Peek first byte; valid packets start with 0x24
      if (peek() == 0x24) {
        uint8_t data[5];
        read_array(data, 5);

        ESP_LOGD("ghome_uart", "Received raw UART: %02X %02X %02X %02X %02X",
                 data[0], data[1], data[2], data[3], data[4]);

        // Valid touch packet if byte2==0x01 and byte4==0x23
        if (data[2] == 0x01 && data[4] == 0x23) {
          int raw_val = data[1];  // ranges roughly 0x01..0x96
          float percent = (raw_val / 150.0f) * 100.0f;  // approximate 0..100%
          if (percent > 100.0f) percent = 100.0f;
          ESP_LOGI("ghome_uart", "Touch Slider → raw: %d → %.1f%%", raw_val, percent);
          slider_sensor->publish_state(percent);
        }
      } else {
        // Not a valid packet start; drop one byte and continue
        read();
      }
    }
  }


 protected:
  sensor::Sensor *slider_sensor;
};