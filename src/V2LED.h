// © Kay Sievers <kay@versioduo.com>, 2020-2022
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <SPI.h>
#include <V2Base.h>

namespace V2LED {

// Simple digital port driver driven by a timer.
class Basic {
public:
  constexpr Basic(uint8_t pin, V2Base::Timer::Periodic *timer) : _pin(pin), _timer(timer) {}
  void tick();
  void setBrightness(float fraction);
  void flash(float seconds, float brightness = 1);
  void loop();
  void reset();

private:
  V2Base::GPIO _pin;
  V2Base::Timer::Periodic *_timer;

  struct {
    unsigned long start_usec{};
    unsigned long duration_usec{};
  } _flash{};
};

// Daisy-chained intelligent RGB-LEDs.
class WS2812 {
public:
  constexpr WS2812(uint16_t n_leds, SPIClass *spi) : _n_leds_max(n_leds), _spi{spi} {}

  // Build SPI bus from SERCOM.
  constexpr WS2812(uint16_t n_leds, uint8_t pin, SERCOM *sercom, SercomSpiTXPad pad_tx, EPioType pin_func) :
    _n_leds_max(n_leds),
    _sercom{.pin{pin}, .sercom{sercom}, .pad_tx{pad_tx}, .pin_func{pin_func}} {}

  void begin();
  void reset();

  // Encodes the DMA bit stream and fires a DMA transaction. If there
  // is a pending update and no current DMA transfer active, a new
  // transaction is started immediately.
  void loop();

  // The logical number of LEDs to drive; it might differ from the number of connected
  // LEDs. The number becomes important when the direction is reversed and the last LED
  // becomes index number zero.
  uint16_t getNumLEDs() const {
    return _leds.count;
  }

  void setNumLEDs(uint16_t count) {
    reset();
    _leds.count = count;
  }

  void setDirection(bool reverse) {
    _leds.reverse = reverse;
  }

  // The fraction of the brightness to apply. The value is applied with
  // the next call to loop().
  void setMaxBrightness(float fraction);

  // Set white color brightness for one or all LEDs.
  void setBrightness(uint16_t index, float v);
  void setBrightness(float v) {
    for (uint16_t i = 0; i < _leds.count; i++)
      setBrightness(i, v);
  }

  // Set HSV color for one or all LEDs.
  void setHSV(uint16_t index, float h, float s, float v);
  void setHSV(float h, float s, float v) {
    for (uint16_t i = 0; i < _leds.count; i++)
      setHSV(i, h, s, v);
  }

  void setRGB(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
  void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < _leds.count; i++)
      setRGB(i, r, g, b);
  }

  // Overlay a timed splash. Sets the color of n LEDs; loop() restores the
  // buffered state after the specified duration.
  void splashHSV(float seconds, uint16_t count, float h, float s, float v);
  void splashHSV(float seconds, float h, float s, float v) {
    splashHSV(seconds, _leds.count, h, s, v);
  }

  // Draw a rainbow, cycles specifies how many cycles through the colors are
  // visible at the same time across all LEDs, seconds is duration for one LED
  // to rotate through one cycle of the colors.
  void rainbow(uint8_t cycles = 1, float seconds = 1, float brightness = 1, bool reverse = false);

private:
  const uint16_t _n_leds_max{};
  struct {
    uint16_t count{};
    bool reverse{};
    float max_brightness{1};
  } _leds;

  struct {
    const uint8_t pin;
    SERCOM *sercom;
    const SercomSpiTXPad pad_tx;
    const EPioType pin_func;
  } _sercom{};

  SPIClass *_spi{};

  struct {
    uint8_t *buffer{};
    uint16_t buffer_size{};
    bool update{};
  } _dma;

  struct PixelRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } *_pixel_rgb{};

  struct PixelDMA {
    uint8_t g[3];
    uint8_t r[3];
    uint8_t b[3];
  } *_pixel_dma{};

  struct {
    PixelRGB pixel;
    uint16_t count;
    unsigned long start_usec;
    unsigned long duration_usec;
  } _splash{};

  struct {
    uint8_t cycle_steps;
    uint8_t move_steps;
    float brightness;
    bool reverse;
    uint16_t color;
    unsigned long update_usec;
    unsigned long last_usec;
  } _rainbow{};

  void setLED(uint16_t index, float h, float s, float v);
  void encodePixel(const struct PixelRGB *rgb, struct PixelDMA *dma);
};
};
