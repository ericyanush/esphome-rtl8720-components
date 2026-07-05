# Custom ESPHome Components

This directory contains standalone ESPHome external components used by this
project.

## Included Components

### `rtl87xx_spi_led_strip`

Custom addressable light output for RTL8720C/LibreTiny targets that drives
WS2811-style LEDs using the Realtek SDK SPI APIs directly.

## How It Is Used

`light.yaml` loads this directory with:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/ericyanush/esphome-rtl8720-components.git
    components: [rtl87xx_spi_led_strip]
```

## Target Hardware Notes

This was developed for a custom RTL8270CM based microcontroller, specifically the module found in some Govee products, however it should work for any microcontroller in the RTL8X family.

## Example

```yaml
light:
  - platform: rtl87xx_spi_led_strip
    id: addressable_light
    name: "Light"
    chipset: WS2811
    num_leds: 13
    rgb_order: GRB
    data_pin: PA19
    clock_pin: PA3
    miso_pin: PA20
    cs_pin: PA17
```
