#include "V2Color.h"
#include "V2LED.h"
#include <wiring_private.h>

void V2LED::begin() {
  // Lead-in of ~300 usec to settle the signal at logic low + pixel data + ~300
  // usec latch 2.4 MBit SPI clock / 8 == 300 kByte/s == 3.33 usec / Byte.
  _dma_buffer_size = 90 + (sizeof(struct PixelDMA) * _n_leds) + 90;
  _dma_buffer      = (uint8_t *)calloc(_dma_buffer_size, 1);

  // Pointer to start of encoded pixel data.
  _pixel_dma = (struct PixelDMA *)(_dma_buffer + 90);

  // RGB buffer to draw DMA pixel data from.
  _pixel_rgb = (struct PixelRGB *)calloc(sizeof(struct PixelRGB), _n_leds);

  // Build SPI bus from SERCOM.
  //
  // SPIClass.begin() applies the board config to all given pins, which might not
  // match our configuration. Just pass the same pin to all of them, to make sure
  // we do not touch anything else. Our pin will be switched to the SERCOM after
  // begin().
  if (!_spi)
    _spi = new SPIClass(_sercom.sercom, _sercom.pin, _sercom.pin, _sercom.pin, _sercom.pad_tx, SERCOM_RX_PAD_3);

  // Configure SPI, the transaction will never stop.
  _spi->begin();
  _spi->beginTransaction(SPISettings(2400000, MSBFIRST, SPI_MODE0));

  if (_sercom.sercom)
    pinPeripheral(_sercom.pin, _sercom.pin_func);

  reset();
}

void V2LED::reset() {
  while (_spi->isBusy())
    yield();

  _splash  = {};
  _rainbow = {};
  setBrightness(0);
}

void V2LED::loop() {
  // Remove timed splash.
  if (_splash.start_usec > 0 && (unsigned long)(micros() - _splash.start_usec) > _splash.duration_usec) {
    _update            = true;
    _splash.start_usec = 0;
  }

  // Draw rainbow.
  if (_rainbow.cycle_steps > 0 && (unsigned long)(micros() - _rainbow.last_usec) > 25 * 1000) {
    _rainbow.last_usec = micros();

    int16_t color = _rainbow.color;
    for (uint16_t i = 0; i < _n_leds; i++) {
      setHSV(i, color, 1, _rainbow.brightness);
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

  if (!_update)
    return;

  if (_spi->isBusy())
    return;

  // Draw splash overlay.
  if (_splash.start_usec > 0) {
    PixelRGB pixel{};

    for (uint16_t i = 0; i < _n_leds; i++) {
      if (i < _splash.n_leds)
        encodePixel(&_splash.pixel, &_pixel_dma[i]);

      else
        encodePixel(&pixel, &_pixel_dma[i]);
    }

  } else {
    for (uint16_t i = 0; i < _n_leds; i++)
      encodePixel(&_pixel_rgb[i], &_pixel_dma[i]);
  }

  _spi->transfer(_dma_buffer, NULL, _dma_buffer_size, false);
  _update = false;
}

static void convertWS2812(float h, float s, float v, uint8_t *rp, uint8_t *gp, uint8_t *bp) {
  uint8_t r, g, b;

  if (v <= 0.f) {
    *rp = *gp = *bp = 0;
    return;
  }

  if (s > 0.f) {
    V2Color::HSVtoRGB(h, s, v, r, g, b);
    r = V2Color::toCIE1931(r);
    g = V2Color::toCIE1931(g);
    b = V2Color::toCIE1931(b);

  } else
    r = g = b = V2Color::toCIE1931(255.f * v);

  // Very low values produce wrong colors for some LEDs.
  if (r > 0 && r < 3)
    r = 3;

  if (g > 0 && g < 3)
    g = 3;

  if (b > 0 && b < 3)
    b = 3;

  *rp = r;
  *gp = g;
  *bp = b;
}

void V2LED::setLED(uint16_t i, float h, float s, float v) {
  convertWS2812(h, s, v, &_pixel_rgb[i].r, &_pixel_rgb[i].g, &_pixel_rgb[i].b);
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

void V2LED::encodePixel(const struct PixelRGB *rgb, struct PixelDMA *dma) {
  encodeByteFrame(rgb->r, dma->r);
  encodeByteFrame(rgb->g, dma->g);
  encodeByteFrame(rgb->b, dma->b);
}

void V2LED::setMaxBrightness(float fraction) {
  _max_brightness = fraction;
  _update         = true;
}

void V2LED::setBrightness(uint16_t i, float v) {
  setLED(i, 0, 0, v * _max_brightness);
  _update = true;
}

void V2LED::setBrightness(float v) {
  for (uint16_t i = 0; i < _n_leds; i++)
    setBrightness(i, v);
}

void V2LED::setHSV(uint16_t i, float h, float s, float v) {
  setLED(i, h, s, v * _max_brightness);
  _update = true;
}

void V2LED::setHSV(float h, float s, float v) {
  for (uint16_t i = 0; i < _n_leds; i++)
    setHSV(i, h, s, v);
}

void V2LED::splashHSV(float seconds, uint16_t n_leds, float h, float s, float v) {
  convertWS2812(h, s, v, &_splash.pixel.r, &_splash.pixel.g, &_splash.pixel.b);
  _splash.n_leds        = n_leds;
  _splash.duration_usec = seconds * 1000 * 1000;
  _splash.start_usec    = micros();
  _update               = true;
}

void V2LED::rainbow(uint8_t cycles, float seconds, float brightness, bool reverse) {
  _rainbow.cycle_steps = (360 / _n_leds) * cycles;
  _rainbow.move_steps  = (360.f / 40.f) / seconds;
  _rainbow.brightness  = brightness;
  _rainbow.reverse     = reverse;
}
