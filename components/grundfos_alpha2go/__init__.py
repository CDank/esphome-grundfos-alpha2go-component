import esphome.codegen as cg
from esphome.components import ble_client

CODEOWNERS = []

CONF_GRUNDFOS_ALPHA2GO_ID = "grundfos_alpha2go_id"

ns = cg.esphome_ns.namespace("grundfos_alpha2go")
GrundfosAlpha2Go = ns.class_(
    "GrundfosAlpha2Go", ble_client.BLEClientNode, cg.PollingComponent
)
SetpointKind = ns.enum("SetpointKind")
SwitchKind = ns.enum("SwitchKind", is_class=True)
ButtonKind = ns.enum("ButtonKind", is_class=True)
