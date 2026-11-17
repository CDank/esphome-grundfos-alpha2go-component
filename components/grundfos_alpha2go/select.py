import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
from esphome.core import CORE

from . import CONF_GRUNDFOS_ALPHA2GO_ID, GrundfosAlpha2Go, ns

GrundfosControlModeSelect = ns.class_(
    "GrundfosControlModeSelect", select.Select, cg.Parented.template(GrundfosAlpha2Go)
)

CONF_CONTROL_MODE = "control_mode"

CONF_LANGUAGE = "language"

# Reihenfolge muss zu SELECT_CONTROL_CODES in grundfos_alpha2go.cpp passen: 1, 0, 13, 14, 8, 2
CONTROL_MODE_OPTIONS = {
    "de": [
        "Prop. Druck",
        "Konstantdruck",
        "Prop. Druck (AutoAdapt)",
        "Konstantdruck (AutoAdapt)",
        "Konstanter Förderstrom",
        "Konstantdrehzahl",
    ],
    "en": [
        "Proportional pressure",
        "Constant pressure",
        "Proportional pressure (AutoAdapt)",
        "Constant pressure (AutoAdapt)",
        "Constant flow",
        "Constant speed",
    ],
}


def _hub_language(hub_id):
    """Sprache aus dem zugehoerigen sensor-Block (platform: grundfos_alpha2go) uebernehmen."""
    for conf in CORE.config.get("sensor", []):
        if conf.get("platform") == "grundfos_alpha2go" and conf[CONF_ID].id == hub_id.id:
            return conf.get(CONF_LANGUAGE, "de")
    return "de"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA2GO_ID): cv.use_id(GrundfosAlpha2Go),
        cv.Optional(CONF_CONTROL_MODE): select.select_schema(
            GrundfosControlModeSelect,
            icon="mdi:tune-vertical",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA2GO_ID])
    if conf := config.get(CONF_CONTROL_MODE):
        options = CONTROL_MODE_OPTIONS[_hub_language(config[CONF_GRUNDFOS_ALPHA2GO_ID])]
        sel = await select.new_select(conf, options=options)
        await cg.register_parented(sel, hub)
        cg.add(hub.set_control_mode_select(sel))
