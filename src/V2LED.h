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
    unsigned long startUsec{};
    unsigned long durationUsec{};
  } _flash{};
};

// Daisy-chained intelligent RGB-LEDs.
class WS2812 {
public:
  constexpr WS2812(uint16_t nLEDs, SPIClass *spi) : _nLEDsMax(nLEDs), _spi{spi} {}

  // Build SPI bus from SERCOM.
  constexpr WS2812(uint16_t nLEDs, uint8_t pin, SERCOM *sercom, SercomSpiTXPad padTX, EPioType pinFunc) :
    _nLEDsMax(nLEDs),
    _sercom{.pin{pin}, .sercom{sercom}, .padTX{padTX}, .pinFunc{pinFunc}} {}

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
  void setBrightness(uint16_t index, float v) {
    if (isRainbow())
      return;

    setLED(index, 0, 0, v);
  }

  void setBrightness(float v) {
    for (uint16_t i = 0; i < _leds.count; i++)
      setBrightness(i, v);
  }

  // Set HSV color for one or all LEDs.
  void setHSV(uint16_t index, float h, float s, float v) {
    if (isRainbow())
      return;

    setLED(index, h, s, v);
  }

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
  bool isRainbow() {
    return _rainbow.cycleSteps > 0;
  }

private:
  const uint16_t _nLEDsMax{};
  struct {
    uint16_t count{};
    bool reverse{};
    float maxBrightness{1};
  } _leds;

  struct {
    const uint8_t pin;
    SERCOM *sercom;
    const SercomSpiTXPad padTX;
    const EPioType pinFunc;
  } _sercom{};

  SPIClass *_spi{};

  struct {
    uint8_t *buffer{};
    uint16_t bufferSize{};
    bool update{};
  } _dma;

  struct PixelRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } * _pixelRGB{};

  struct PixelDMA {
    uint8_t g[3];
    uint8_t r[3];
    uint8_t b[3];
  } * _pixelDMA{};

  struct {
    PixelRGB pixel;
    uint16_t count;
    unsigned long startUsec;
    unsigned long durationUsec;
  } _splash{};

  struct {
    uint8_t cycleSteps;
    uint8_t moveSteps;
    float brightness;
    bool reverse;
    uint16_t color;
    unsigned long updateUsec;
    unsigned long lastUsec;
  } _rainbow{};

  void setLED(uint16_t index, float h, float s, float v);
  void encodePixel(const struct PixelRGB *rgb, struct PixelDMA *dma);
};
};
