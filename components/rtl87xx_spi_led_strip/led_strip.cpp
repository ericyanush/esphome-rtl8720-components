#include "led_strip.h"

#ifdef USE_RTL87XX

#include <cinttypes>
#include <Arduino.h>

#include "esphome/core/log.h"

// This is necessary as for some reason without undefining these symbols and providing a basic
// implementation of them, code using the SPI HAL fails to link as these symbols are missing.
#ifdef DBG_SSI_ERR
#undef DBG_SSI_ERR
#endif
#ifdef DBG_SSI_INFO
#undef DBG_SSI_INFO
#endif

extern "C" {
void DBG_SSI_ERR(const char *format, ...) { (void) format; }
void DBG_SSI_INFO(const char *format, ...) { (void) format; }
}

namespace esphome::rtl87xx_spi_led_strip {

static const char *const TAG = "rtl87xx_spi_led_strip";

void RTL87XXSPILEDStripLightOutput::setup() {
  const size_t buffer_size = this->get_buffer_size_();
  const size_t encoded_buffer_size = this->get_encoded_buffer_size_();

  RAMAllocator<uint8_t> allocator;
  this->buf_ = allocator.allocate(buffer_size);
  this->effect_data_ = allocator.allocate(this->num_leds_);
  this->encoded_buf_ = allocator.allocate(encoded_buffer_size);

  if (this->buf_ == nullptr || this->effect_data_ == nullptr || this->encoded_buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate LED buffers");
    this->mark_failed();
    return;
  }

  memset(this->buf_, 0, buffer_size);
  memset(this->effect_data_, 0, this->num_leds_);
  memset(this->encoded_buf_, 0, encoded_buffer_size);

  if (!this->setup_spi_()) {
    this->mark_failed();
    return;
  }
}

light::LightTraits RTL87XXSPILEDStripLightOutput::get_traits() {
  auto traits = light::LightTraits();
  if (this->is_rgbw_ || this->is_wrgb_) {
    traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE});
  } else {
    traits.set_supported_color_modes({light::ColorMode::RGB});
  }
  return traits;
}

void RTL87XXSPILEDStripLightOutput::set_led_params(uint8_t bit0, uint8_t bit1, uint32_t spi_frequency) {
  this->bit0_ = bit0;
  this->bit1_ = bit1;
  this->spi_frequency_ = spi_frequency;
}

void RTL87XXSPILEDStripLightOutput::clear_effect_data() {
  if (this->effect_data_ != nullptr) {
    memset(this->effect_data_, 0, this->num_leds_);
  }
}

void RTL87XXSPILEDStripLightOutput::encode_() {
  uint8_t *dst = this->encoded_buf_ + this->reset_size_;
  const size_t buffer_size = this->get_buffer_size_();

  for (size_t index = 0; index < buffer_size; index++) {
    const uint8_t value = this->buf_[index];
    for (uint8_t bit = 0; bit < 8; bit++) {
      *dst++ = value & (1 << (7 - bit)) ? this->bit1_ : this->bit0_;
    }
  }
}

PinName RTL87XXSPILEDStripLightOutput::pin_name_(uint8_t pin) const {
  PinInfo *info = pinInfo(pin);
  if (info == nullptr) {
    return NC;
  }
  return static_cast<PinName>(info->gpio);
}

bool RTL87XXSPILEDStripLightOutput::setup_spi_() {
  const PinName mosi = this->pin_name_(this->data_pin_);
  const PinName miso = this->pin_name_(this->miso_pin_);
  const PinName sck = this->pin_name_(this->clock_pin_);
  const PinName cs = this->pin_name_(this->cs_pin_);

  if (mosi == NC || miso == NC || sck == NC || cs == NC) {
    ESP_LOGE(TAG, "Invalid SPI pin mapping mosi=%u miso=%u sck=%u cs=%u", this->data_pin_, this->miso_pin_,
             this->clock_pin_, this->cs_pin_);
    return false;
  }

  spi_init(&this->spi_, mosi, miso, sck, cs);
  spi_format(&this->spi_, 8, 0, 0);
  spi_frequency(&this->spi_, this->spi_frequency_);
  this->spi_initialized_ = true;

  ESP_LOGI(TAG, "Initialized SPI ring bus mosi=%u miso=%u sck=%u cs=%u", this->data_pin_, this->miso_pin_,
           this->clock_pin_, this->cs_pin_);
  return true;
}

void RTL87XXSPILEDStripLightOutput::write_state(light::LightState *state) {
  if (this->is_failed()) {
    return;
  }

  const uint32_t now = micros();
  if (this->max_refresh_rate_.has_value() && *this->max_refresh_rate_ != 0 &&
      (now - this->last_refresh_) < *this->max_refresh_rate_) {
    this->schedule_show();
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  if (!this->spi_initialized_) {
    ESP_LOGE(TAG, "SPI bus is not initialized");
    this->status_set_warning();
    return;
  }

  this->encode_();
  if (spi_master_write_stream_dma(&this->spi_, reinterpret_cast<char *>(this->encoded_buf_),
                                  this->get_encoded_buffer_size_()) != HAL_OK) {
    ESP_LOGE(TAG, "SPI DMA transfer setup failed");
    this->status_set_warning();
    return;
  }
  while (this->spi_.state & SPI_STATE_TX_BUSY) {
    yield();
  }
  while (spi_busy(&this->spi_)) {
    yield();
  }
}

light::ESPColorView RTL87XXSPILEDStripLightOutput::get_view_internal(int32_t index) const {
  int32_t r = 0;
  int32_t g = 0;
  int32_t b = 0;
  switch (this->rgb_order_) {
    case ORDER_RGB:
      r = 0;
      g = 1;
      b = 2;
      break;
    case ORDER_RBG:
      r = 0;
      g = 2;
      b = 1;
      break;
    case ORDER_GRB:
      r = 1;
      g = 0;
      b = 2;
      break;
    case ORDER_GBR:
      r = 2;
      g = 0;
      b = 1;
      break;
    case ORDER_BGR:
      r = 2;
      g = 1;
      b = 0;
      break;
    case ORDER_BRG:
      r = 1;
      g = 2;
      b = 0;
      break;
  }

  const uint8_t multiplier = this->is_rgbw_ || this->is_wrgb_ ? 4 : 3;
  const uint8_t white = this->is_wrgb_ ? 0 : 3;

  return {this->buf_ + (index * multiplier) + r + this->is_wrgb_,
          this->buf_ + (index * multiplier) + g + this->is_wrgb_,
          this->buf_ + (index * multiplier) + b + this->is_wrgb_,
          this->is_rgbw_ || this->is_wrgb_ ? this->buf_ + (index * multiplier) + white : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

void RTL87XXSPILEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "RTL87XX SPI LED Strip:");
  ESP_LOGCONFIG(TAG, "  LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Data rate: %" PRIu32, this->spi_frequency_);
  ESP_LOGCONFIG(TAG, "  Data pin: %u", this->data_pin_);
  ESP_LOGCONFIG(TAG, "  Clock pin: %u", this->clock_pin_);
  ESP_LOGCONFIG(TAG, "  MISO pin: %u", this->miso_pin_);
  ESP_LOGCONFIG(TAG, "  CS pin: %u", this->cs_pin_);
  if (this->max_refresh_rate_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Max refresh rate: %" PRIu32, *this->max_refresh_rate_);
  }
}

}  // namespace esphome::rtl87xx_spi_led_strip

#endif  // USE_RTL87XX
