// © Kay Sievers <kay@versioduo.com>, 2020-2022
// SPDX-License-Identifier: Apache-2.0

#include "V2Color.h"
#include "V2LED.h"
#include <wiring_private.h>

void V2LED::WS2812::begin() {
  // Lead-in of ~300 usec to settle the signal at logic low + pixel data + ~300
  // usec latch 2.4 MBit SPI clock / 8 == 300 kByte/s == 3.33 usec / Byte.
  _dma.buffer_size = 90 + (sizeof(struct PixelDMA) * _n_leds_max) + 90;
  _dma.buffer      = (uint8_t *)calloc(_dma.buffer_size, 1);

  // Pointer to start of encoded pixel data.
  _pixel_dma = (struct PixelDMA *)(_dma.buffer + 90);

  // RGB buffer to draw DMA pixel data from.
  _pixel_rgb = (struct PixelRGB *)calloc(sizeof(struct PixelRGB), _n_leds_max);

  // Build SPI bus from SERCOM.
  //
  // SPIClass.begin() applies the board config to all given pins, which might not
  // match our configuration. Just pass the same pin to all of them, to make sure
  // we do not touch anything else. Our pin will be switched to the SERCOM after
  // begin().
  if (!_spi)
    _spi = new SPIClass(_sercom.sercom, _sercom.pin, _sercom.pin, _sercom.pin, _sercom.pad_tx, SERCOM_RX_PAD_3);

  if (_sercom.sercom)
    pinPeripheral(_sercom.pin, _sercom.pin_func);

  // Configure SPI, the transaction will never stop.
  _spi->begin();
  _spi->beginTransaction(SPISettings(2400000, MSBFIRST, SPI_MODE0));

  _leds.count = _n_leds_max;
  reset();
}

void V2LED::WS2812::reset() {
  while (_spi->isBusy())
    yield();

  _splash  = {};
  _rainbow = {};
  setBrightness(0);
}

void V2LED::WS2812::loop() {
  // Remove timed splash.
  if (_splash.start_usec > 0 && (unsigned long)(micros() - _splash.start_usec) > _splash.duration_usec) {
    _dma.update        = true;
    _splash.start_usec = 0;
  }

  // Draw rainbow.
  if (_rainbow.cycle_steps > 0 && (unsigned long)(micros() - _rainbow.last_usec) > 25 * 1000) {
    _rainbow.last_usec = micros();

    int16_t color = _rainbow.color;
    for (uint16_t i = 0; i < _leds.count; i++) {
      setLED(i, color, 1, _rainbow.brightness);

      if (_rainbow.reverse) {
        color += _rainbow.cycle_steps;
        if (color > 359)
          color -= 360;

      } else {
        color -= _rainbow.cycle_steps;
        if (color < 0)
          color += 360;
      }
    }

    _rainbow.color += _rainbow.move_steps;
    if (_rainbow.color > 359)
      _rainbow.color -= 360;
  }

  if (!_dma.update)
    return;

  if (_spi->isBusy())
    return;

  // Draw splash overlay.
  if (_splash.start_usec > 0) {
    PixelRGB pixel{};

    for (uint16_t i = 0; i < _leds.count; i++) {
      PixelDMA *pixel_dma = _leds.reverse ? &_pixel_dma[_leds.count - 1 - i] : &_pixel_dma[i];
      if (i < _splash.count)
        encodePixel(&_splash.pixel, pixel_dma);

      else
        encodePixel(&pixel, &_pixel_dma[i]);
    }
  } else {
    for (uint16_t i = 0; i < _leds.count; i++) {
      PixelDMA *pixel_dma = _leds.reverse ? &_pixel_dma[_leds.count - 1 - i] : &_pixel_dma[i];
      encodePixel(&_pixel_rgb[i], pixel_dma);
    }
  }

  _spi->transfer(_dma.buffer, NULL, _dma.buffer_size, false);
  _dma.update = false;
}

static void convertWS2812(float h, float s, float v, uint8_t *rp, uint8_t *gp, uint8_t *bp) {
  uint8_t r, g, b;

  if (v <= 0.f) {
    *rp = *gp = *bp = 0;
    return;
  }

  if (s > 0.f)
    V2Color::HSVtoRGB(h, s, V2Color::toCIE1931(v), r, g, b);

  else
    r = g = b = ceilf(255.f * V2Color::toCIE1931(v));

  *rp = r;
  *gp = g;
  *bp = b;
}

void V2LED::WS2812::setLED(uint16_t index, float h, float s, float v) {
  convertWS2812(h, s, v * _leds.max_brightness, &_pixel_rgb[index].r, &_pixel_rgb[index].g, &_pixel_rgb[index].b);
  _dma.update = true;
}

static void encodeByteFrame(uint8_t b, uint8_t f[3]) {
  union {
    uint32_t bits;
    uint8_t bytes[4];
  } frame{.bits{0b100100100100100100100100}};

  // Encode 1 bit into 3 bits frame data, 0b100 == 0, 0b110 == 1
  if (b & 1)
    frame.bits |= 1 << 1;

  if (b & 2)
    frame.bits |= 1 << 4;

  if (b & 4)
    frame.bits |= 1 << 7;

  if (b & 8)
    frame.bits |= 1 << 10;

  if (b & 16)
    frame.bits |= 1 << 13;

  if (b & 32)
    frame.bits |= 1 << 16;

  if (b & 64)
    frame.bits |= 1 << 19;

  if (b & 128)
    frame.bits |= 1 << 22;

  f[0] = frame.bytes[2];
  f[1] = frame.bytes[1];
  f[2] = frame.bytes[0];
}

void V2LED::WS2812::encodePixel(const struct PixelRGB *rgb, struct PixelDMA *dma) {
  encodeByteFrame(rgb->r, dma->r);
  encodeByteFrame(rgb->g, dma->g);
  encodeByteFrame(rgb->b, dma->b);
}

void V2LED::WS2812::setMaxBrightness(float fraction) {
  _leds.max_brightness = fraction;
  _dma.update          = true;
}

void V2LED::WS2812::setRGB(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (isRainbow())
    return;

  _pixel_rgb[index].r = r;
  _pixel_rgb[index].g = g;
  _pixel_rgb[index].b = b;
  _dma.update         = true;
}

void V2LED::WS2812::splashHSV(float seconds, uint16_t count, float h, float s, float v) {
  convertWS2812(h, s, v, &_splash.pixel.r, &_splash.pixel.g, &_splash.pixel.b);
  _splash.count         = count;
  _splash.duration_usec = seconds * 1000 * 1000;
  _splash.start_usec    = micros();
  _dma.update           = true;
}

void V2LED::WS2812::rainbow(uint8_t cycles, float seconds, float brightness, bool reverse) {
  _rainbow.cycle_steps = (360 / _leds.count) * cycles;
  _rainbow.move_steps  = (360.f / 40.f) / seconds;
  _rainbow.brightness  = brightness;
  _rainbow.reverse     = reverse;
}
