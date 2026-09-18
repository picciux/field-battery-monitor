
#ifndef _HARDWARE_H
#define _HARDWARE_H

#include "battery.h"
#include "settings.h"


class PwmPin {
    private:
        int pin;
        uint8_t value;
    public:
        void setup(int pinNumber);
        int getByteValue();
        float getValue();
        void setByteValue(uint8_t value);
        void setValue(float value);
        void turnOn(bool on);
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
        void setBrightness(float brightness);
        void setBrightness(float brightness, unsigned int transitionDurationMs);
        float getBrightness();
        unsigned long getDefaultTransision();
        void setDefaultTransition(unsigned long transitionDurationMs);
        void turnOn();
        void turnOff();
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
    public:
        void setBrightness(float brightness); 
        bool isAutoEnabled();
        void autoEnable(bool enable);
        float getAutoBrightness();
        void setAutoBrightness(float brightness);
        int getAutoDuration();
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
    public:
        float getTemperature();
        float getLowThreshold();
        void setLowThreshold(float c);
        void setup(Settings *settings);
        void run(unsigned long now);
};

class Hardware {
    public:
        Battery *battery;
        Light *light;
        Heater *heater;
        PwmPin **outlets;
        Settings *settings;

        int getOutletsNum();

        void setup(Settings *settings);
        void run(unsigned long now);
};

#endif // _HARDWARE_H