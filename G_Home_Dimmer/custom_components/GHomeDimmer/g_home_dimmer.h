#pragma once

namespace esphome::GHomeDimmer {

    class GHomeDimmerComponent : public Component, public UARTDevice {
        public:
            GHomeDimmerComponent();

            void setup() override;
            void dump_config() override;
    }
}