#pragma once

#ifdef USE_RTL87XX

#include "sdk_private.h"
#include <ArduinoPrivate.h>

extern "C" {
#include "spi_api.h"
#include "spi_ex_api.h"
void hal_ssi_toggle_between_frame(phal_ssi_adaptor_t phal_ssi_adaptor, u8 ctl);
}

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::rtl87xx_spi_led_strip {

enum RGBOrder : uint8_t {
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BGR,
  ORDER_BRG,
};

class RTL87XXSPILEDStripLightOutput : public light::AddressableLight {
 public:
  explicit RTL87XXSPILEDStripLightOutput(uint16_t num_leds) : num_leds_(num_leds) {}

  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override { return setup_priority::IO; }

  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override;
  void dump_config() override;

  void set_is_rgbw(bool is_rgbw) { this->is_rgbw_ = is_rgbw; }
  void set_is_wrgb(bool is_wrgb) { this->is_wrgb_ = is_wrgb; }
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }
  void set_led_params(uint8_t bit0, uint8_t bit1, uint32_t spi_frequency);
  void set_data_pin(uint8_t data_pin) { this->data_pin_ = data_pin; }
  void set_clock_pin(uint8_t clock_pin) { this->clock_pin_ = clock_pin; }
  void set_miso_pin(uint8_t miso_pin) { this->miso_pin_ = miso_pin; }
  void set_cs_pin(uint8_t cs_pin) { this->cs_pin_ = cs_pin; }
  void set_rgb_order(RGBOrder rgb_order) { this->rgb_order_ = rgb_order; }

  void clear_effect_data() override;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t get_buffer_size_() const { return this->num_leds_ * (this->is_rgbw_ || this->is_wrgb_ ? 4 : 3); }
  size_t get_encoded_buffer_size_() const { return this->get_buffer_size_() * 8 + this->reset_size_ * 2; }
  void encode_();
  bool setup_spi_();
  PinName pin_name_(uint8_t pin) const;

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint8_t *encoded_buf_{nullptr};

  uint16_t num_leds_;
  bool is_rgbw_{false};
  bool is_wrgb_{false};

  uint8_t data_pin_{0};
  uint8_t clock_pin_{0};
  uint8_t miso_pin_{20};
  uint8_t cs_pin_{15};
  spi_t spi_{};
  bool spi_initialized_{false};

  uint32_t spi_frequency_{6666666};
  uint8_t bit0_{0xE0};
  uint8_t bit1_{0xFC};
  RGBOrder rgb_order_{ORDER_GRB};

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
  size_t reset_size_{64};
};

}  // namespace esphome::rtl87xx_spi_led_strip

#endif  // USE_RTL87XX
