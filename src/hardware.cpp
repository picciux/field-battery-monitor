#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "include_config.h"
#include "hardware.h"
#include "pins.h"

#define TRANSITION_FPS        50
#define TRANSITION_PWM_DELAY  ( 1000 / TRANSITION_FPS) // in milliseconds


/* temperature sensor */
OneWire oneWire(PIN_ONE_WIRE);
DallasTemperature sensors(&oneWire);

void PwmPin::setup(int pinNumber)
{
  this->pin = pinNumber;
  pinMode(this->pin, OUTPUT);
  this->setByteValue(0);
}

int PwmPin::getByteValue()
{
  return this->value;
}

float PwmPin::getValue()
{
  return this->value / 255.0f;
}

bool PwmPin::setByteValue(uint8_t value)
{
  if (value == this->value) return false;
  analogWrite(this->pin, value);
  this->value = value;
  return true;
}

bool PwmPin::setValue(float value)
{
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  return this->setByteValue((int) (value * 255.0f));
}

bool PwmPin::turnOn(bool on)
{
  return this->setByteValue(on ? 255 : 0);
}

void BaseLight::_setBrightness(float brightness)
{
  this->brightness = brightness;
  if (brightness > 0)
    this->lastBrightness = brightness;
  this->pin.setValue(brightness);
  this->on = (brightness > 0.0f);
}

bool BaseLight::setBrightness(float brightness, unsigned int transitionDurationMs)
{
  if (this->brightness == brightness) return false;
  if (transitionDurationMs == 0) {
    _setBrightness(brightness);
    transitioning = false;
  } else {
    this->transition = transitionDurationMs;
    startBrightness = this->brightness;
    targetBrightness = brightness;
    transitionStart = millis();
    transitioning = true;
  }

  return true;
}

bool BaseLight::setBrightness(float brightness) {
  return setBrightness(brightness, defaultTransition);
}

float BaseLight::getBrightness()
{
  return this->brightness;
}

unsigned long BaseLight::getDefaultTransision()
{
  return this->defaultTransition;
}

void BaseLight::setDefaultTransition(unsigned long transitionDurationMs)
{
  this->defaultTransition = transitionDurationMs;
}

bool BaseLight::turnOn()
{
  if (this->lastBrightness > 0)
    return this->setBrightness(this->lastBrightness);
  else
    return this->setBrightness(1.0);
}

bool BaseLight::turnOff()
{
  return this->setBrightness(0.0f);
}

void BaseLight::setup(int pin)
{
    this->pin.setup(pin);
}

void BaseLight::run(unsigned long now) 
{
  if (!transitioning) return;
  
  /* throttle down */
  if (now - lastPwmUpdate < TRANSITION_PWM_DELAY) return;
  lastPwmUpdate = now;

  float progress = (float) (now - transitionStart) / (transition * 1000.0f);
  if (progress >= 1.0) {
    _setBrightness(targetBrightness);
    transitioning = false;
    return;
  }

  /* TODO: gamma 2.2 interpolation insetead of linear */
  _setBrightness(startBrightness + progress * (targetBrightness - startBrightness));
}

void Light::setBrightness(float brightness)
{
  this->autoEnabled = false;
  if (BaseLight::setBrightness(brightness) && _listener)
    _listener->onHardwareChanged(HardwareEvent::Light, 0);
}

bool Light::isAutoEnabled()
{
  return this->autoEnabled;
}

void Light::autoEnable(bool enable)
{
  if (enable != autoEnabled) {
    this->autoEnabled = enable;
    settings->setAutoLightEnabled(enable);
    if (_listener)
      _listener->onHardwareChanged(HardwareEvent::Light, 0);
  }   
}

float Light::getAutoBrightness()
{
  return this->autoBrightness;
}

void Light::setAutoBrightness(float brightness)
{
  if (brightness != autoBrightness) {
    this->autoBrightness = brightness;
    settings->setAutoLightBrightness(brightness);
    if (_listener)
      _listener->onHardwareChanged(HardwareEvent::Light, 0);
  }
}

int Light::getAutoDuration()
{
  return this->autoDuration;
}

void Light::setAutoDuration(int seconds)
{
  if (seconds != autoDuration) {
    this->autoDuration = seconds;
    settings->setAutoLightDuration(seconds);
    if (_listener)
      _listener->onHardwareChanged(HardwareEvent::Light, 0);
  }
}

void Light::setup(int pwmPin, int pirPin, Settings *settings)
{
    BaseLight::setup(pwmPin);
    pinMode(pirPin, INPUT);
    this->pirPin = pirPin;
    this->settings = settings;

    this->autoEnabled = settings->isAutoLightEnabled();
    this->autoBrightness = settings->getAutoLightBrightness();
    this->autoDuration = settings->getAutoLightDuration();
}

void Light::run(unsigned long now) 
{
 bool is_moving = digitalRead(pirPin);

 if (this->autoEnabled) {
    if (is_moving) {
      if (this->autoTime == 0) {
        if (! this->on) {
          BaseLight::setBrightness(this->autoBrightness);
          this->autoTime = now;
          if (_listener)
            _listener->onHardwareChanged(HardwareEvent::Light, 0);
        }
      } else {
        this->autoTime = now;
      }
  } else {
      if (this->autoTime > 0 && (now - this->autoTime >= this->autoDuration * 1000)) {
        BaseLight::setBrightness(0.0f);
        if (_listener)
          _listener->onHardwareChanged(HardwareEvent::Light, 0);
      }
    }
  }
  
  BaseLight::run(now);
}

float Heater::getTemperature()
{
    return this->temperature;
}

float Heater::getLowThreshold()
{
    return this->lowThreshold;
}

void Heater::setLowThreshold(float c)
{
    if (c != lowThreshold) {
      this->lowThreshold = c;
      settings->setCpLowThreshold(c);
      if (_listener)
        _listener->onHardwareChanged(HardwareEvent::Heater, 0);
    }
}

void Heater::setup(Settings *settings)
{
  this->pin.setup(PIN_MOSFET_HEATER);
  this->settings = settings;
  this->lowThreshold = settings->getCpLowThreshold();
  sensors.begin();
}

void Heater::run(unsigned long now)
{
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C) {
    this->temperature = t;
  }

  /* Anti-ice automation */
  if (this->temperature <= this->lowThreshold) {
    this->pin.turnOn(true);
  } else if (this->temperature >= this->lowThreshold + HEATER_HYSTERESIS_C) {
    this->pin.turnOn(false);
  }
}

void PowerOutlet::setPower(float power) {
  if (power != _pin.getValue()) {
    _pin.setValue(power);
    if (_listener)
      _listener->onHardwareChanged(HardwareEvent::Outlet, _index);
  }
}

void Led::setBrightness(float brightness) {
  this->blinking = false;
  BaseLight::setBrightness(brightness);
}

void Led::run(unsigned long now) {
  if (! blinking) {
    BaseLight::run(now);
    return;
  }

  /* blinking code */
  switch(blinkState) {
    case BLINK_ON:
      if (now - blinkLast >= onMs) {
        BaseLight::setBrightness(0.0);
        blinkState = BLINK_OFF;
        blinkLast = now;
      }
      break;

    case BLINK_OFF:
      if (now - blinkLast >= offMs) {
        BaseLight::setBrightness(0.0);
        if (blinkRc < repeat) {
          BaseLight::setBrightness(blinkBrightness);
          blinkState = BLINK_ON;
          blinkRc++;
        } else {
          blinkState = BLINK_PAUSE;
          blinkRc = 1;
        }
        blinkLast = now;
      }
      break;

    case BLINK_PAUSE:
      if (now - blinkLast >= pauseMs) {
        BaseLight::setBrightness(blinkBrightness);
        blinkState = BLINK_ON;
        blinkLast = now;
      }
      break;
  }
}

Battery _battery;
Heater _heater;
Led _led;

#ifdef DISABLE_LIGHT
#ifdef CHANNELS_4
PowerOutlet _out1;
PowerOutlet _out2;
PowerOutlet _out3;
PowerOutlet *_outlets[] = {
  &_out1, &_out2, &_out3
};
#define OUTLETS_COUNT   3
#else
PowerOutlet out1;
PowerOutlet *outlets[] = {
  &_out1
};
#define OUTLETS_COUNT   1
#endif //CHANNELS_4
#else
Light _light;
#ifdef CHANNELS_4
PowerOutlet _out1;
PowerOutlet _out2;
PowerOutlet *_outlets[] = {
  &_out1, &_out2
};
#define OUTLETS_COUNT   2
#else
PowerOutlet *_outlets[] = {};
#define OUTLETS_COUNT    0
#endif //CHANNELS_4
#endif //DISABLE_LIGHT

void Hardware::setup(Settings *settings)
{
    this->battery = &_battery;
    this->heater = &_heater;
    this->outlets = _outlets;
    this->led = &_led;

    this->settings = settings;

    this->battery->setup(BATTERY_CAPACITY);
    this->heater->setup(settings);

#ifdef DISABLE_LIGHT
#ifdef CHANNELS_4
    this->outlets[0]->setup(PIN_MOSFET_LIGHT, 0);
    this->outlets[1]->setup(PIN_MOSFET_CH3, 1);
    this->outlets[2]->setup(PIN_MOSFET_CH4, 2);
#else
    this->outlets[0]->setup(PIN_MOSFET_LIGHT, 0);
#endif //CHANNELS_4
#else
    this->light = &_light;
    this->light->setup(PIN_MOSFET_LIGHT, PIN_PIR, settings);
#ifdef CHANNELS_4
    this->outlets[0]->setup(PIN_MOSFET_CH3, 0);
    this->outlets[1]->setup(PIN_MOSFET_CH4, 1);
#endif //CHANNELS_4
#endif //DISABLE_LIGHT

  this->led->setup(PIN_LED);
}

int Hardware::getOutletsNum() 
{
 return OUTLETS_COUNT;
}

void Hardware::run(unsigned long now)
{
    this->battery->run(now);
    this->heater->run(now);
    this->light->run(now);
    this->led->run(now);
}
    



