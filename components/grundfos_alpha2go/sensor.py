import esphome.codegen as cg
from esphome.components import ble_client, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_FLOW,
    CONF_HEAD,
    CONF_ID,
    CONF_POWER,
    CONF_SPEED,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_HOUR,
    UNIT_METER,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_VOLT,
    UNIT_WATT,
)

DEPENDENCIES = ["ble_client"]

CONF_DC_VOLTAGE = "dc_voltage"
CONF_AUX_TEMPERATURE = "aux_temperature"
CONF_MEDIA_TEMPERATURE = "media_temperature"
CONF_PCB_TEMPERATURE = "pcb_temperature"
CONF_CONTROLBOX_TEMPERATURE = "controlbox_temperature"
CONF_STARTS = "starts"
CONF_OPERATING_HOURS = "operating_hours"
CONF_POWER_ON_HOURS = "power_on_hours"

ns = cg.esphome_ns.namespace("grundfos_alpha2go")
GrundfosAlpha2Go = ns.class_(
    "GrundfosAlpha2Go", ble_client.BLEClientNode, cg.PollingComponent
)


def _temp():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GrundfosAlpha2Go),
            cv.Optional(CONF_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HEAD): sensor.sensor_schema(
                unit_of_measurement=UNIT_METER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DC_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AUX_TEMPERATURE): _temp(),
            cv.Optional(CONF_MEDIA_TEMPERATURE): _temp(),
            cv.Optional(CONF_PCB_TEMPERATURE): _temp(),
            cv.Optional(CONF_CONTROLBOX_TEMPERATURE): _temp(),
            cv.Optional(CONF_STARTS): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                icon="mdi:counter",
            ),
            cv.Optional(CONF_OPERATING_HOURS): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=1,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                icon="mdi:timer-outline",
            ),
            cv.Optional(CONF_POWER_ON_HOURS): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=1,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                icon="mdi:timer-outline",
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_FLOW, var.set_flow_sensor)
    await sensors(CONF_HEAD, var.set_head_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_DC_VOLTAGE, var.set_dc_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_SPEED, var.set_speed_sensor)
    await sensors(CONF_AUX_TEMPERATURE, var.set_aux_temperature_sensor)
    await sensors(CONF_MEDIA_TEMPERATURE, var.set_media_temperature_sensor)
    await sensors(CONF_PCB_TEMPERATURE, var.set_pcb_temperature_sensor)
    await sensors(CONF_CONTROLBOX_TEMPERATURE, var.set_controlbox_temperature_sensor)
    await sensors(CONF_STARTS, var.set_starts_sensor)
    await sensors(CONF_OPERATING_HOURS, var.set_operating_hours_sensor)
    await sensors(CONF_POWER_ON_HOURS, var.set_power_on_hours_sensor)
