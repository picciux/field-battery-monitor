
#include <Arduino.h>
#include <Wire.h>
#include <INA226_WE.h>

#include "pins.h"
#include "include_config.h"
#include "battery.h"
#include "soc_persistance.h"
#include "current_averager.h"

#define CURRENT_SAMPLE_DELAY_MS     1000
#define AUTONOMY_SAMPLE_DELAY_MS    30000
#define AVERAGER_WINDOW_MS          600000ul
#define AVERAGER_CAPACITY           (AVERAGER_WINDOW_MS / CURRENT_SAMPLE_DELAY_MS)

// Busses and sensors
INA226_WE ina = INA226_WE(INA226_ADDR); // Indirizzo I2C standard dell'INA226
CurrentAverager ca(AVERAGER_CAPACITY, AVERAGER_WINDOW_MS);

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

float Battery::getAutonomyHours() {
    return autonomyH;
}

void Battery::setChangeListener(IHardwareChangeListener* listener)
{ 
    _listener = listener; 
}

void Battery::updateSafety()
{
    bool wasSafe = isSafe();

    if (soc <= SAFETY_SOC_LOW) _socUnsafe = true;
    else if (soc >= SAFETY_SOC_RECOVER) _socUnsafe = false;

    if (voltage <= SAFETY_CRITICAL_VOLTAGE_V) _voltageUnsafe = true;
    else if (voltage >= SAFETY_CRITICAL_VOLTAGE_V + SAFETY_CRITICAL_VOLTAGE_HYST_V) _voltageUnsafe = false;

    if (isSafe() != wasSafe && _listener)
        _listener->onHardwareChanged(HardwareEvent::Safety, 0);
}

void Battery::setup(float capacity)
{
  // Start I2C bus
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  
  _sensorValid = ina.init();
  if (_sensorValid) {
    // INA226 config
    ina.setResistorRange(INA226_RESISTOR, INA226_RANGE); 
    ina.setCorrectionFactor(1.0);
    // 4-sample internal average
    ina.setAverage(INA226_AVERAGE_4);
  }

  socPersistance.setup(SOC_PERSIST_MAX_TIME, SOC_PERSIST_MAX_DIFF);
  
  this->soc = socPersistance.recover();
  this->capacity = capacity;
  this->voltage = 0.0f;
  this->current = 0.0f;
  this->_last_update = millis();
  this->_last_autonomy = _last_update;
}

void Battery::run(unsigned long now)
{
    if ((now - _last_update) < CURRENT_SAMPLE_DELAY_MS) return;

    float delta_hours = (now - this->_last_update) / 3600000.0f;
    _last_update = now;

    Wire.beginTransmission(INA226_ADDR);
    _sensorValid = (Wire.endTransmission() == 0);
    
    if (!_sensorValid) {
        // Tensione, corrente e SoC restano fermi all'ultimo valore noto.
        if (_listener)
            _listener->onHardwareChanged(HardwareEvent::BatteryMainData, 0);
        return;
    }
    
    // 1. Lettura Tensione e Corrente dall'INA226
    ina.readAndClearFlags();
    voltage = ina.getBusVoltage_V();

    // Convenzione di progetto: l'INA226 (cablato con + verso la batteria) legge
    // positivo in scarica. Invertiamo subito: da qui in poi negativo = scarica,
    // positivo = carica.
    current = ina.getCurrent_A() * -1.0f;

    ca.addSample(now, current);

    
    float integratedA = (fabs(current) > SOC_MIN_CURRENT_INTEGRATION) ? current : 0.0f;
    soc += (integratedA / capacity) * 100.0f * delta_hours;
    if (soc > 100.0f) soc = 100.0f;
    if (soc < 0.0f) soc = 0.0f;

    // SoC reset evaluation. If voltage stays over reset threshold and
    // current stays under reset threshold for enough time, SoC is reset
    // to 100% and immediately saved.
    const float tailA = capacity * SOC_RESET_TAIL_CURRENT;
    if (soc < 100.0f) {
        if (fabs(current) <= tailA && voltage >= SOC_RESET_VOLTAGE) {
            if (_start_soc_reset_condition == 0)
                _start_soc_reset_condition = now;
            else if (now - _start_soc_reset_condition >= SOC_RESET_TIME) {
                reset();
                _start_soc_reset_condition = 0;
            }            
        } else {
            _start_soc_reset_condition = 0;
        }
    }

    socPersistance.update(soc, now);
    updateSafety();
    if (_listener)
        _listener->onHardwareChanged(HardwareEvent::BatteryMainData, 0);

    if ( (now - _last_autonomy) >= AUTONOMY_SAMPLE_DELAY_MS ) {  
        float currentAvg = 0.0;
        if (ca.getAverage(now, currentAvg)) {
            _last_autonomy = now;
            autonomyH = BatteryAutonomy::computeHours(getRemainingAh(), currentAvg);
            if (_listener)
                _listener->onHardwareChanged(HardwareEvent::BatteryAutonomy, 0);
        }
    }
}

void Battery::reset() {
    soc = 100.0f;
    _start_soc_reset_condition = 0;
    socPersistance.force(soc, millis());  
}