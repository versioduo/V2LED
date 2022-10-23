// © Kay Sievers <kay@versioduo.com>, 2020-2022
// SPDX-License-Identifier: Apache-2.0

#include "V2LED.h"
#include <wiring_private.h>

void V2LED::Basic::tick() {
  if (!_timer->isFraction())
    _pin.high();

  else
    _pin.low();
}

void V2LED::Basic::setBrightness(float fraction) {
  if (fraction <= 0) {
    _flash = {};
    _timer->setFraction(0);
    _timer->disable();
    _pin.low();
    return;
  }

  if (fraction >= 1) {
    _timer->setFraction(0);
    _timer->disable();
    _pin.high();
    return;
  }

  _timer->setFraction(fraction);
  _timer->enable();
}

void V2LED::Basic::flash(float seconds, float brightness) {
  _flash.start_usec    = micros();
  _flash.duration_usec = seconds * 1000.f * 1000.f;
  setBrightness(brightness);
}

void V2LED::Basic::loop() {
  if (_flash.duration_usec == 0)
    return;

  if ((unsigned long)(micros() - _flash.start_usec) < _flash.duration_usec)
    return;

  _flash.duration_usec = 0;
  setBrightness(0);
}

void V2LED::Basic::reset() {
  _flash = {};
  _timer->setFraction(0);
  _timer->disable();
  _pin.low();
}
