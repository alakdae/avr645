import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor, sensor
from esphome.const import CONF_ID, CONF_UART_ID, CONF_PORT

DEPENDENCIES = ['uart']

avr645_ns = cg.esphome_ns.namespace('avr645')
AVR645 = avr645_ns.class_('AVR645', cg.Component)

CONF_UPPER_ID = "upper_id"
CONF_LOWER_ID = "lower_id"
CONF_VOLUME_ID = "volume_id"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AVR645),
    cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
    cv.Required(CONF_UPPER_ID): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_LOWER_ID): cv.use_id(text_sensor.TextSensor),
    cv.Optional(CONF_VOLUME_ID): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_PORT, default=4001): cv.port,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    parent = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(parent))
    
    upper = await cg.get_variable(config[CONF_UPPER_ID])
    cg.add(var.set_upper_sensor(upper))
    
    lower = await cg.get_variable(config[CONF_LOWER_ID])
    cg.add(var.set_lower_sensor(lower))

    if CONF_VOLUME_ID in config:
      volume = await cg.get_variable(config[CONF_VOLUME_ID])
      cg.add(var.set_volume_sensor(volume))

    cg.add(var.set_port(config[CONF_PORT]))
