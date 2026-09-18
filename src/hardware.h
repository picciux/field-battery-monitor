
#ifndef _HARDWARE_H
#define _HARDWARE_H

#include "battery.h"
#include "settings.h"
#include "ihardware_change_listener.h"

class PwmPin {
    private:
        int pin;
        uint8_t value;
    public:
        void setup(int pinNumber);
        int getByteValue();
        float getValue();
        bool setByteValue(uint8_t value);
        bool setValue(float value);
        bool turnOn(bool on);
};

class BaseLight {
    private:
        unsigned long defaultTransition = 0;
        unsigned long transition = 0;
        float startBrightness = 0.0f;
        float targetBrightness = 0.0f;
        unsigned long transitionStart = 0;
        unsigned long lastPwmUpdate = 0;
        bool transitioning = false;
        float lastBrightness = 0;
        void _setBrightness(float brightness);

    protected:
        float brightness = 0;
        bool on = false;
        PwmPin pin;
    public:
        bool setBrightness(float brightness);
        bool setBrightness(float brightness, unsigned int transitionDurationMs);
        float getBrightness();
        unsigned long getDefaultTransision();
        void setDefaultTransition(unsigned long transitionDurationMs);
        bool turnOn();
        bool turnOff();
        void setup(int pin);
        void run(unsigned long now);
};

class Light : public BaseLight {
    private:
        bool autoEnabled = false;
        unsigned long autoTime = 0;
        float autoBrightness;
        int autoDuration;
        int pirPin;
        Settings *settings;
        IHardwareChangeListener *_listener;
    public:
        void setBrightness(float brightness); 
        bool isAutoEnabled();
        void autoEnable(bool enable);
        float getAutoBrightness();
        void setAutoBrightness(float brightness);
        int getAutoDuration();
        void setChangeListener(IHardwareChangeListener *listener) { _listener = listener; };
        void setAutoDuration(int seconds);
        void setup(int pwmPin, int pirPin, Settings *settings);
        void run(unsigned long now);
};

class Heater {
    private:
        float temperature;
        float lowThreshold;
        PwmPin pin;
        Settings *settings;
        IHardwareChangeListener *_listener;
    public:
        float getTemperature();
        float getLowThreshold();
        void setLowThreshold(float c);
        void setHardwareChangeListener(IHardwareChangeListener *listener) { _listener = listener; }
        void setup(Settings *settings);
        void run(unsigned long now);
};

class PowerOutlet {
    private:
        PwmPin _pin;
        IHardwareChangeListener *_listener;
        int _index;
    public:
        void setPower(float power);
        float getPower() { return _pin.getValue(); }
        void setHardwareChangeListener(IHardwareChangeListener *listener) { _listener = listener; }
        void setup(int pinNumber, int index) { _pin.setup(pinNumber); _index = index; }
};

class Hardware {
    public:
        Battery *battery;
        Light *light;
        Heater *heater;
        PowerOutlet **outlets;
        Settings *settings;

        int getOutletsNum();

        void setup(Settings *settings);
        void run(unsigned long now);
};

#endif // _HARDWARE_H