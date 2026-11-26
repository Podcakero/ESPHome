import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID

DEPENDENCIES = ["output"]

color_temp_rgbw_ns = cg.esphome_ns.namespace("color_temp_rgbw_light")
ColorTempRGBWLight = color_temp_rgbw_ns.class_("ColorTempRGBWLight", cg.Component)

CONF_RED = "red"
CONF_GREEN = "green"
CONF_BLUE = "blue"
CONF_WHITE = "white"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ColorTempRGBWLight),
        cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
        cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
        cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
        cv.Required(CONF_WHITE): cv.use_id(output.FloatOutput),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    red = await cg.get_variable(config[CONF_RED])
    green = await cg.get_variable(config[CONF_GREEN])
    blue = await cg.get_variable(config[CONF_BLUE])
    white = await cg.get_variable(config[CONF_WHITE])
    
    cg.add(var.set_red(red))
    cg.add(var.set_green(green))
    cg.add(var.set_blue(blue))
    cg.add(var.set_white(white))
