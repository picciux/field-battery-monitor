import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

alpaca_ns = cg.esphome_ns.namespace('alpaca')
AlpacaComponent = alpaca_ns.class_('AlpacaComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(AlpacaComponent),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(cg.RawStatement('#include "esphome/components/alpaca/alpaca.h"'))

