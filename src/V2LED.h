// © Kay Sievers <kay@vrfy.org>, 2020-2021
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <SPI.h>

class V2LED {
public:
  constexpr V2LED(uint16_t n_leds, SPIClass *spi) : _n_leds(n_leds), _spi{spi} {}

  // Build SPI bus from SERCOM.
  constexpr V2LED(uint16_t n_leds, uint8_t pin, SERCOM *sercom, SercomSpiTXPad pad_tx, EPioType pin_func) :
    _n_leds(n_leds),
    _sercom{.pin{pin}, .sercom{sercom}, .pad_tx{pad_tx}, .pin_func{pin_func}} {}

  void begin();
  void reset();

  // Encodes the DMA bit stream and fires a DMA transaction. If there
  // is a pending update and no current DMA transfer active, a new
  // transaction is started immediately.
  void loop();

  // The fraction of the brightness to apply. The value is applied with
  // the next call to loop().
  void setMaxBrightness(float fraction);
  uint16_t getNumLEDs() {
    return _n_leds;
  }

  // Set white color brightness for one or all LEDs.
  void setBrightness(uint16_t i, float v);
  void setBrightness(float v);

  // Set HSV color for one or all LEDs.
  void setHSV(uint16_t i, float h, float s, float v);
  void setHSV(float h, float s, float v);

  // Overlay a timed splash. Sets the color of n LEDs; loop() restores the
  // buffered state after the specified duration.
  void splashHSV(float seconds, uint16_t n_leds, float h, float s, float v);
  void splashHSV(float seconds, float h, float s, float v) {
    splashHSV(seconds, _n_leds, h, s, v);
  }

  // Draw a rainbow, cycles specifies how many cycles through the colors are
  // visible at the same time across all LEDs, seconds is duration for one LED
  // to rotate through one cycle of the colors.
  void rainbow(uint8_t cycles = 1, float seconds = 1, float brightness = 1, bool reverse = false);

private:
  const uint16_t _n_leds;

  struct {
    const uint8_t pin;
    SERCOM *sercom;
    const SercomSpiTXPad pad_tx;
    const EPioType pin_func;
  } _sercom{};

  SPIClass *_spi{};

  float _max_brightness = 1;

  uint8_t *_dma_buffer{};
  uint16_t _dma_buffer_size{};
  bool _update{};

  struct PixelRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } * _pixel_rgb{};

  struct PixelDMA {
    uint8_t g[3];
    uint8_t r[3];
    uint8_t b[3];
  } * _pixel_dma{};

  struct {
    PixelRGB pixel;
    uint16_t n_leds;
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

  void setLED(uint16_t i, float h, float s, float v);
  void encodePixel(const struct PixelRGB *rgb, struct PixelDMA *dma);
};
