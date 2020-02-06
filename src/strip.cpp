#include <Adafruit_NeoPixel.h>

class Strip {
    public:
        uint8_t   effect;
        uint8_t   effects;
        uint16_t  effStep;
        unsigned long effStart;
        Adafruit_NeoPixel strip;

    Strip(uint16_t leds, uint8_t pin, uint8_t toteffects, uint16_t striptype) : strip(leds, pin, striptype) {
        effect = -1;
        effects = toteffects;
        Reset();
    }

    void Reset(){
        effStep = 0;
        effect = (effect + 1) % effects;
        effStart = millis();
    }
};

struct Loop {
    uint8_t currentChild;
    uint8_t childs;
    bool timeBased;
    uint16_t cycles;
    uint16_t currentTime;
    Loop(uint8_t totchilds, bool timebased, uint16_t tottime) {currentTime=0;currentChild=0;childs=totchilds;timeBased=timebased;cycles=tottime;}
};