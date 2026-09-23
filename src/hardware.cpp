#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "include_config.h"
#include "hardware.h"
#include "pins.h"
#include "board.h"    


#define TRANSITION_FPS        50
#define TRANSITION_PWM_DELAY  ( 1000 / TRANSITION_FPS) // in milliseconds
#define TRANSITION_UPD_FRAMES 10 // call transition update every TRANSITION_UPD_FRAMES transition frames

#define TEMP_SAMPLE_INTERVAL_MS   5000   // ogni quanto avviare una conversione
#define TEMP_CONVERSION_MS         800   // DS18B20 a 12 bit: max 750 ms
#define TEMP_STALE_TIMEOUT_MS    60000   // oltre, il dato e' considerato non valido

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

bool BaseLight::setBrightness(float brightness, unsigned long transitionDurationMs)
{
  if (this->brightness == brightness) return false;
  if (transitionDurationMs == 0) {
    _setBrightness(brightness);
    transitioning = false;
  } else {
    this->transition = transitionDurationMs;
    startBrightness = this->brightness;
    targetBrightness = brightness;
    transitionFrame = 0;
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

  float elapsed = (now >= transitionStart) ? (float)(now - transitionStart) : 0.0f ;
  float progress = elapsed / transition;
  if (progress >= 1.0) {
    _setBrightness(targetBrightness);
    transitioning = false;
    transitionUpdate(1.0);
    return;
  }

  /* TODO: gamma 2.2 interpolation insetead of linear */
  _setBrightness(startBrightness + progress * (targetBrightness - startBrightness));
  
  if ((++transitionFrame % TRANSITION_UPD_FRAMES) == 0)
    transitionUpdate(progress); //TODO throttle based on TRANSITION_UPD_FRAMES
}

void Light::setBrightness(float brightness)
{
  this->autoActive = false;
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
    if (!enable && autoActive) {
      autoActive = false;
      BaseLight::setBrightness(0.0f);
    }
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

void Light::transitionUpdate(float progress)
{
  if (_listener)
    _listener->onHardwareChanged(HardwareEvent::Light, 0);
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
  if (autoEnabled) {
    bool isMoving = digitalRead(pirPin);

    if (isMoving) {
      if (!autoActive) {
        // non tocco la luce se e' gia' accesa (es. da comando manuale)
        if (!on) {
          BaseLight::setBrightness(autoBrightness);
          autoActive = true;
          autoTime = now;
          if (_listener)
            _listener->onHardwareChanged(HardwareEvent::Light, 0);
        }
      } else {
        autoTime = now;   // movimento continuo: rinnova il timer
      }
    } else if (autoActive &&
               (now - autoTime) >= (unsigned long) autoDuration * 1000UL) {
      BaseLight::setBrightness(0.0f);
      autoActive = false;
      if (_listener)
        _listener->onHardwareChanged(HardwareEvent::Light, 0);
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
  sensors.setWaitForConversion(false);   // niente attese bloccanti
}

void Heater::run(unsigned long now)
{
  /* 1. Lettura temperatura, non bloccante */
  if (!conversionPending) {
    if (lastRequest == 0 || (now - lastRequest) >= TEMP_SAMPLE_INTERVAL_MS) {
      sensors.requestTemperatures();
      lastRequest = now;
      conversionPending = true;
    }
  } else if ((now - lastRequest) >= TEMP_CONVERSION_MS) {
    conversionPending = false;
    float t = sensors.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C) {
      temperature = t;
      tempValid = true;
      lastValidRead = now;
    }
  }

  /* dato troppo vecchio (sensore scollegato/guasto) */
  if (tempValid && (now - lastValidRead) > TEMP_STALE_TIMEOUT_MS) {
    tempValid = false;
  }

  /* 2. Automazione anti-freddo. Sempre attiva; senza dato valido
        il riscaldatore resta spento (failsafe) */
  if (!tempValid) {
    pin.turnOn(false);
  } else if (temperature <= lowThreshold) {
    pin.turnOn(true);
  } else if (temperature >= lowThreshold + HEATER_HYSTERESIS_C) {
    pin.turnOn(false);
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
Light _light;
PowerOutlet _outletObjs[MAX_OUTLETS];
PowerOutlet *_outletPtrs[MAX_OUTLETS];

void Hardware::setup(Settings *settings)
{
    this->battery = &_battery;
    this->heater = &_heater;
    this->led = &_led;
    this->settings = settings;

    this->battery->setup(BATTERY_CAPACITY);
    this->heater->setup(settings);

    if (HAS_LIGHT) {
        this->light = &_light;
        this->light->setup(PIN_MOSFET_LIGHT, PIN_PIR, settings);
        this->light->setDefaultTransition(LIGHT_DEFAULT_TRANSITION_MS);
    } else {
        this->light = nullptr;
    }

    for (int i = 0; i < OUTLET_COUNT; i++) {
        _outletPtrs[i] = &_outletObjs[i];
        _outletPtrs[i]->setup(CHANNEL_PINS[FIRST_OUTLET_CHANNEL + i], i);
    }
    this->outlets = _outletPtrs;

    this->led->setup(PIN_LED);
}

int Hardware::getOutletsNum()
{
    return OUTLET_COUNT;
}

void Hardware::run(unsigned long now)
{
    this->battery->run(now);
    this->heater->run(now);
    if (this->light) this->light->run(now);
    this->led->run(now);
}


