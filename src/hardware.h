// Global variables

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

class Light {
    private:
        float brightness = 0;
        bool autoEnabled = false;
        unsigned long auto_time = 0;
        float autoBrightness;
        int autoDuration;
        bool on = false;

        void _hw_set_brightness(float b);
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
        float highThreshold;

    public:
        float getTemperature();
        float getLowThreshold();
        float getHighThreshold();
        void setLowThreshold(float c);
        void setHighThreshold(float c);
        void setup();
        void run(unsigned long now);
};

class Hardware {
    public:
        Battery *battery;
        Light *light;
        Heater *heater;

        void setup();
        void run();
};

