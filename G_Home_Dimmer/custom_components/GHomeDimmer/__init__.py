import esphome.codegen as cg
from esphome.components import esp32, uart
import esphome.config_validation as cv
from esphome.const import {
    CONF_ID,
    CONF_NAME
}
from esphome.core import CORE

DEPENDENCIES = ["uart"]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsMeterComponent),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_provider_deprecation,
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("g_home_dimmer", require_rx=True)


async def to_code(config):