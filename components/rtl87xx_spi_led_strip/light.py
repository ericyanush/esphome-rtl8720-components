from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import libretiny, light
from esphome.components.libretiny.const import FAMILY_RTL8720C
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_CHIPSET,
    CONF_CS_PIN,
    CONF_IS_RGBW,
    CONF_MAX_REFRESH_RATE,
    CONF_MISO_PIN,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_DATA_PIN,
    CONF_RGB_ORDER,
)

CODEOWNERS = ["@ericyanush"]
DEPENDENCIES = ["libretiny"]

rtl87xx_spi_led_strip_ns = cg.esphome_ns.namespace("rtl87xx_spi_led_strip")
RTL87XXSPILEDStripLightOutput = rtl87xx_spi_led_strip_ns.class_(
    "RTL87XXSPILEDStripLightOutput", light.AddressableLight
)

RGBOrder = rtl87xx_spi_led_strip_ns.enum("RGBOrder")

RGB_ORDERS = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BGR": RGBOrder.ORDER_BGR,
    "BRG": RGBOrder.ORDER_BRG,
}


@dataclass
class LEDStripTimings:
    bit0: int
    bit1: int
    spi_frequency: int


CHIPSETS = {
    "WS2811": LEDStripTimings(0b11100000, 0b11111100, 6666666),
    "WS2812": LEDStripTimings(0b11100000, 0b11111100, 6666666),
    "WS2812B": LEDStripTimings(0b11100000, 0b11111100, 6666666),
    "SK6812": LEDStripTimings(0b11000000, 0b11111000, 7500000),
}

CONF_IS_WRGB = "is_wrgb"


def _validate_family(config):
    return libretiny.only_on_family(supported=[FAMILY_RTL8720C])(config)


def _validate_num_leds(value):
    max_num_leds = 512
    if value[CONF_IS_RGBW] or value[CONF_IS_WRGB]:
        max_num_leds = 384
    if value[CONF_NUM_LEDS] > max_num_leds:
        raise cv.Invalid(
            f"The maximum number of LEDs for this configuration is {max_num_leds}.",
            path=CONF_NUM_LEDS,
        )
    return value


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RTL87XXSPILEDStripLightOutput),
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_RGB_ORDER): cv.enum(RGB_ORDERS, upper=True),
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CLOCK_PIN, default=8): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_MISO_PIN, default=20): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_CS_PIN, default=15): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_IS_RGBW, default=False): cv.boolean,
            cv.Optional(CONF_IS_WRGB, default=False): cv.boolean,
        }
    ),
    _validate_family,
    _validate_num_leds,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    if (max_refresh_rate := config.get(CONF_MAX_REFRESH_RATE)) is not None:
        cg.add(var.set_max_refresh_rate(max_refresh_rate))

    chipset = CHIPSETS[config[CONF_CHIPSET]]
    cg.add(var.set_led_params(chipset.bit0, chipset.bit1, chipset.spi_frequency))
    cg.add(var.set_data_pin(config[CONF_DATA_PIN]))
    cg.add(var.set_clock_pin(config[CONF_CLOCK_PIN]))
    cg.add(var.set_miso_pin(config[CONF_MISO_PIN]))
    cg.add(var.set_cs_pin(config[CONF_CS_PIN]))

    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))
    cg.add(var.set_is_rgbw(config[CONF_IS_RGBW]))
    cg.add(var.set_is_wrgb(config[CONF_IS_WRGB]))
