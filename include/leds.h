#ifndef __leds_h
#define __leds_h

#include <Arduino.h>
#include "leds-data.h"

typedef enum LEDEnum
{
    LED_LEV20,
    LED_LEV40,
    LED_LEV60,
    LED_LEV80,
    LED_LEV100,
    LED_S_NODE,
    LED_S_BATT,
    LED_STAT,
    LED_POUT,
    LED_STAT2,  
    LED_Count
} LEDEnum;

#define LED_OVERRIDE_INFINITE 0xffffffff

//callback function indicating a script hit the end. If Finished is true then the script
//is done running otherwise it is looping
typedef void (*LEDCallbackFunc)(const uint16_t ID, bool Finished);

class LEDs
{
    public:
        virtual void AddScript(uint8_t ID, const uint8_t *data, uint16_t len);
        virtual void StartScript(uint16_t ID, bool TempOverride);
        virtual void StopScript(uint16_t ID);
        virtual void SetGlobalVariable(uint8_t ID, float Value);
        virtual void SetLocalVariable(uint16_t ID, uint8_t VarID, float Value);
        virtual float GetLEDValue(LEDEnum LED);
        virtual void SetLEDValue(LEDEnum LED, float Value, uint32_t LengthMS);
        virtual uint16_t GetAmbientSensor();
        virtual void SetLEDBrightness(float BrightnessPercent);
        virtual float GetLEDBrightness();
        virtual void SetLEDCap(uint8_t Count);
};

//LED initializer
LEDs *NewLEDs(LEDCallbackFunc Callback, bool DisableThread);

#endif