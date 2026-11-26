import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT_ID

DEPENDENCIES = ["output"]
MULTI_CONF = False

color_temp_rgbw_ns = cg.esphome_ns.namespace("color_temp_rgbw_light")
ColorTempRGBWLight = color_temp_rgbw_ns.class_(
    "ColorTempRGBWLight", cg.Component, light.LightOutput
)

CONF_RED = "red"
CONF_GREEN = "green"
CONF_BLUE = "blue"
CONF_WHITE = "white"

# RGB_LIGHT_SCHEMA matches the previous `custom` light platform
# (which used ADDRESSABLE_LIGHT_SCHEMA): gamma_correct, default_transition_length,
# flash_transition_length and RGB effects are available.
CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ColorTempRGBWLight),
        cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
        cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
        cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
        cv.Required(CONF_WHITE): cv.use_id(output.FloatOutput),
    }
)


async def to_code(config):
    # The user-facing `id` becomes the LightState; the LightOutput uses CONF_OUTPUT_ID.
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    # Register the output as its own Component so setup()/loop() run here
    # (needed for the low-brightness kickstart dim-down). Its ID is
    # CONF_OUTPUT_ID, so this does not clash with the LightState registered below.
    await cg.register_component(var, config)
    await light.register_light(var, config)

    red = await cg.get_variable(config[CONF_RED])
    green = await cg.get_variable(config[CONF_GREEN])
    blue = await cg.get_variable(config[CONF_BLUE])
    white = await cg.get_variable(config[CONF_WHITE])

    cg.add(var.set_red(red))
    cg.add(var.set_green(green))
    cg.add(var.set_blue(blue))
    cg.add(var.set_white(white))
