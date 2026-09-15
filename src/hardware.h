
#ifndef _HARDWARE_H
#define _HARDWARE_H

class Battery {
    private:
        float soc;
        float capacity;
        float voltage;
        float current;
        unsigned long _last_soc_time;
    public:
        float getSoC();
        float getCapacity();
        float getVoltage();
        float getCurrent();
        void setup();
        void run(unsigned long now);
        /* Reset SoC to full 100%. */
        void reset();
};

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

class Light {
    private:
        float brightness = 0;
        bool autoEnabled = false;
        unsigned long auto_time = 0;
        float autoBrightness;
        int autoDuration;
        bool on = false;
        PwmPin pin;

        //void _hw_set_brightness(float b);
    public:
        void setBrightness(float brightness);
        float getBrightness();
        bool isAutoEnabled();
        void autoEnable(bool enable);
        float getAutoBrightness();
        void setAutoBrightness(float brightness);
        int getAutoDuration();
        void setAutoDuration(int seconds);
        void turnOn();
        void turnOff();
        void setup();
        void run(unsigned long now);
};

class Heater {
    private:
        float temperature;
        float lowThreshold;
        PwmPin pin;

    public:
        float getTemperature();
        float getLowThreshold();
        void setLowThreshold(float c);
        void setup();
        void run(unsigned long now);
};

class Hardware {
    public:
        Battery *battery;
        Light *light;
        Heater *heater;
        PwmPin **outlets;

        void setup();
        void run();
};

#endif // _HARDWARE_H