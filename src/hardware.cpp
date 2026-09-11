#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "include_config.h"
#include "hardware.h"

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

void Battery::setup()
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

  this->soc = 100.0f;
  this->capacity = BATTERY_CAPACITY;
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
}

void Battery::reset() {
  this->soc = 100.0f;
}

void Light::_hw_set_brightness(float b)
{
  analogWrite(PIN_MOSFET_LIGHT, (int) (b * 255.0));
}

void Light::setBrightness(float brightness)
{
  this->autoEnabled = false;
  this->brightness = brightness;
  this->_hw_set_brightness(brightness);
  this->on = (brightness > 0.0f);
}

float Light::getBrightness()
{
  return this->brightness;
}

bool Light::isAutoEnabled()
{
  return this->autoEnabled;
}

void Light::autoEnable(bool enable)
{
  this->autoEnabled = enable;
}

float Light::getAutoBrightness()
{
  return this->autoBrightness;
}

void Light::setAutoBrightness(float brightness)
{
  this->autoBrightness = brightness;
}

int Light::getAutoDuration()
{
  return this->autoDuration;
}

void Light::setAutoDuration(int seconds)
{
  this->autoDuration = seconds;
}

void Light::turnOn()
{
  if (this->brightness > 0)
    this->setBrightness(this->brightness);
  else
    this->setBrightness(1.0);
}

void Light::turnOff()
{
  this->_hw_set_brightness(0.0);
  this->on = false;
}

void Light::setup()
{
    pinMode(PIN_MOSFET_LIGHT, OUTPUT);
    pinMode(PIN_PIR, INPUT);
}

void Light::run(unsigned long now) 
{
 bool is_moving = digitalRead(PIN_PIR);

 if (this->autoEnabled) {
    if (is_moving) {
        if (this->auto_time == 0) {
            if (! this->on) {
                this->_hw_set_brightness(this->autoBrightness);
                this->auto_time = now;
            }
        } else {
          this->auto_time = now;
        }
    } else {
        if (this->auto_time > 0 && (now - this->auto_time >= this->autoDuration * 1000)) {
            this->turnOff();
        }
    }
  }  
}

float Heater::getTemperature()
{
    return this->temperature;
}

float Heater::getLowThreshold()
{
    return this->lowThreshold;
}

float Heater::getHighThreshold()
{
    return this->highThreshold;
}

void Heater::setLowThreshold(float c)
{
    this->lowThreshold = c;
}

void Heater::setHighThreshold(float c)
{
    this->highThreshold = c;
}

void Heater::setup()
{
  pinMode(PIN_MOSFET_HEATER, OUTPUT);
  analogWrite(PIN_MOSFET_HEATER, 0); // Riscaldatore spento al boot
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
    analogWrite(PIN_MOSFET_HEATER, 255); // turn-on
  } else if (this->temperature >= this->highThreshold) {
    analogWrite(PIN_MOSFET_HEATER, 0);   // turn-off
  }
}

Battery _battery;
Light _light;
Heater _heater;

void Hardware::setup()
{
    this->battery = &_battery;
    this->heater = &_heater;
    this->light = &_light;

    this->battery->setup();
    this->heater->setup();
    this->light->setup();
}

void Hardware::run()
{
    unsigned long now = millis();
    this->battery->run(now);
    this->light->run(now);
    this->heater->run(now);
};

Hardware hardware;


