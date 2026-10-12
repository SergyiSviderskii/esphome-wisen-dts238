import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, time
from esphome.const import CONF_ID, CONF_TIME_ID

DEPENDENCIES = ["uart"]

smdst2387_ns = cg.esphome_ns.namespace("smdst2387")
SmDST2387 = smdst2387_ns.class_(
    "SmDST2387", cg.Component, uart.UARTDevice
)

CONF_SMDTS2387_ID = "smdts2387_id"
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SmDST2387),
    cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_id(time_)) 
        
        