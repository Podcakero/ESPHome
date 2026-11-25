import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID
from esphome.components import uart, light

DEPENDENCIES = ["uart"]

ghome_dimmer_ns = cg.esphome_ns.namespace("ghome_dimmer_uart_slider")
GHomeDimmerUartSlider = ghome_dimmer_ns.class_(
    "GHomeDimmerUartSlider", cg.Component, uart.UARTDevice
)

CONF_SWITCH_LIGHT = "switch_light"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GHomeDimmerUartSlider),
        cv.Optional(CONF_SWITCH_LIGHT): cv.use_id(light.LightState),
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
    if CONF_SWITCH_LIGHT in config:
        light_state = await cg.get_variable(config[CONF_SWITCH_LIGHT])
        cg.add(var.set_switch_light(light_state))