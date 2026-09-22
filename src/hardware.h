
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
        bool setBrightness(float brightness, unsigned long transitionDurationMs);
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
        bool autoActive = false; 
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
        float temperature = 0.0f;
        bool tempValid = false;
        bool conversionPending = false;
        unsigned long lastRequest = 0;
        unsigned long lastValidRead = 0;
        float lowThreshold;
        PwmPin pin;
        Settings *settings;
        IHardwareChangeListener *_listener = nullptr;
    public:
        float getTemperature();
        bool isTemperatureValid() const { return tempValid; }
        bool isOn() { return pin.getByteValue() > 0; }
        float getLowThreshold();
        void setLowThreshold(float c);
        void setChangeListener(IHardwareChangeListener *listener) { _listener = listener; }
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
        void setChangeListener(IHardwareChangeListener *listener) { _listener = listener; }
        void setup(int pinNumber, int index) { _pin.setup(pinNumber); _index = index; }
};

class Led : public BaseLight {
    private:
        static constexpr int BLINK_ON = 0;
        static constexpr int BLINK_OFF = 1;
        static constexpr int BLINK_PAUSE = 2;

        bool blinking = false;
        int blinkState = BLINK_ON;
        unsigned long blinkLast;
        int blinkRc = 1;
        unsigned long onMs;
        unsigned long offMs;
        int repeat = 1;
        unsigned long pauseMs;
        float blinkBrightness;

    public:
        bool isBlinking() const { return blinking; }
        void startBlink(float brightness, unsigned long onMs_, unsigned long offMs_, int repeat = 1, 
            unsigned long pause = 0) {
            onMs = onMs_;
            offMs = offMs_;
            if (repeat < 1) repeat = 1;
            this->repeat = repeat;
            this->pauseMs = pause;
            this->blinkBrightness = brightness;
            this->blinking = true;
            blinkState = BLINK_ON;
            BaseLight::setBrightness(brightness);
            blinkRc = 1;
            blinkLast = millis();
        }
        void setBrightness(float brightness);
        void run(unsigned long now);
};

class Hardware {
    public:
        Battery *battery;
        Light *light;
        Heater *heater;
        PowerOutlet **outlets;
        Led *led;
        Settings *settings;

        int getOutletsNum();

        void setup(Settings *settings);
        void run(unsigned long now);
};

#endif // _HARDWARE_H