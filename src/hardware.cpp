#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "include_config.h"
#include "hardware.h"
#include "soc_persistance.h"

// PIN Mapping
#define PIN_BTN              0
#define PIN_PIR             13
#define PIN_ONE_WIRE        25
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         19
#define PIN_MOSFET_HEATER   16
#define PIN_MOSFET_LIGHT    17
#define PIN_MOSFET_CH3      26
#define PIN_MOSFET_CH4      27
#define PIN_LED             23

#define TRANSITION_FPS        50
#define TRANSITION_PWM_DELAY  ( 1000 / TRANSITION_FPS) // in milliseconds

// Busses and sensors
INA226_WE ina = INA226_WE(INA226_ADDR); // Indirizzo I2C standard dell'INA226
OneWire oneWire(PIN_ONE_WIRE);
DallasTemperature sensors(&oneWire);

float Battery::getSoC()
{
    return this->soc;
}

float Battery::getCapacity()
{
    return this->capacity;
}

float Battery::getVoltage()
{
    return this->voltage;
}

float Battery::getCurrent()
{
    return this->current;
}

float Battery::getRemainingAh() {
    return capacity * soc / 100.0f;
}

void Battery::setup(float capacity, Settings *settings)
{
  // Start I2C bus
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  
  if (ina.init()) {
    // INA226 conig
    ina.setResistorRange(INA226_RESISTOR, INA226_RANGE); 
    ina.setCorrectionFactor(1.0);
    // 4-sample internal average
    ina.setAverage(INA226_AVERAGE_4);
  }

  this->settings = settings;

  socPersistance.setup(SOC_PERSIST_MAX_TIME, SOC_PERSIST_MAX_DIFF);
  
  this->soc = socPersistance.recover();
  this->capacity = capacity;
  this->voltage = 0.0f;
  this->current = 0.0f;
  this->_last_soc_time = millis();
}

void Battery::run(unsigned long now)
{
  // 1. Lettura Tensione e Corrente dall'INA226
  ina.readAndClearFlags();
  this->voltage = ina.getBusVoltage_V();
  
  // Moltiplichiamo per -1.0f per invertire il segno come nel tuo YAML
  this->current = ina.getCurrent_A() * -1.0f; 

  // 4. Calcolo SoC (Integrazione dei Coulomb)
  float delta_hours = (now - this->_last_soc_time) / 3600000.0f;
  this->_last_soc_time = now;
  
  this->soc += (this->current / this->capacity) * 100.0f * delta_hours;
  if (this->soc > 100.0f) this->soc = 100.0f;
  if (this->soc < 0.0f) this->soc = 0.0f;

  socPersistance.update(this->soc, now);
}

void Battery::reset() {
  this->soc = 100.0f;
}

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

void PwmPin::setByteValue(uint8_t value)
{
  if (value == this->value) return;
  analogWrite(this->pin, value);
  this->value = value;
}

void PwmPin::setValue(float value)
{
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  this->setByteValue((int) (value * 255.0f));
}

void PwmPin::turnOn(bool on)
{
  this->setByteValue(on ? 255 : 0);
}

void BaseLight::_setBrightness(float brightness)
{
  this->brightness = brightness;
  if (brightness > 0)
    this->lastBrightness = brightness;
  this->pin.setValue(brightness);
  this->on = (brightness > 0.0f);
}

void BaseLight::setBrightness(float brightness, unsigned int transitionDurationMs)
{
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
}

void BaseLight::setBrightness(float brightness) {
  setBrightness(brightness, defaultTransition);
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

void BaseLight::turnOn()
{
  if (this->lastBrightness > 0)
    this->setBrightness(this->lastBrightness);
  else
    this->setBrightness(1.0);
}

void BaseLight::turnOff()
{
  this->setBrightness(0.0f);
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
  BaseLight::setBrightness(brightness);
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
        }
      } else {
        this->autoTime = now;
      }
  } else {
      if (this->autoTime > 0 && (now - this->autoTime >= this->autoDuration * 1000)) {
        BaseLight::setBrightness(0.0f);
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

Battery _battery;
Heater _heater;

#ifdef DISABLE_LIGHT
#ifdef CHANNELS_4
PwmPin _out1;
PwmPin _out2;
PwmPin _out3;
PwmPin *_outlets[] = {
  &_out1, &_out2, &_out3
};
#define OUTLETS_COUNT   3
#else
PwmPin __aligned_;
PwmPin *outlets[] = {
  &_out1
};
#define OUTLETS_COUNT   1
#endif //CHANNELS_4
#else
Light _light;
#ifdef CHANNELS_4
PwmPin _out1;
PwmPin _out2;
PwmPin *_outlets[] = {
  &_out1, &_out2
};
#define OUTLETS_COUNT   2
#else
PwmPin *_outlets[] = {};
#define OUTLETS_COUNT    0
#endif //CHANNELS_4
#endif //DISABLE_LIGHT

void Hardware::setup(Settings *settings)
{
    this->battery = &_battery;
    this->heater = &_heater;
    this->outlets = _outlets;

    this->settings = settings;

    this->battery->setup(BATTERY_CAPACITY, settings);
    this->heater->setup(settings);

#ifdef DISABLE_LIGHT
#ifdef CHANNELS_4
    this->outlets[0]->setup(PIN_MOSFET_LIGHT);
    this->outlets[1]->setup(PIN_MOSFET_CH3);
    this->outlets[2]->setup(PIN_MOSFET_CH4);
#else
    this->outlets[0]->setup(PIN_MOSFET_LIGHT);
#endif //CHANNELS_4
#else
    this->light = &_light;
    this->light->setup(PIN_MOSFET_LIGHT, PIN_PIR, settings);
#ifdef CHANNELS_4
    this->outlets[0]->setup(PIN_MOSFET_CH3);
    this->outlets[1]->setup(PIN_MOSFET_CH4);
#endif //CHANNELS_4
#endif //DISABLE_LIGHT

}

int Hardware::getOutletsNum() 
{
 return OUTLETS_COUNT;
}

void Hardware::run(unsigned long now)
{
    static unsigned long last_battery = 0;
    
    if (now - last_battery >= 1000) {
      last_battery = now;
      this->battery->run(now);
      this->heater->run(now);
    }

    this->light->run(now);
}
    



