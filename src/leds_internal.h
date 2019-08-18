#ifndef _leds_internal_h
#define _leds_internal_h

#include "leds.h"

#define LED_LEV20_PIN 25
#define LED_LEV40_PIN 26
#define LED_LEV60_PIN 27
#define LED_LEV80_PIN 14
#define LED_LEV100_PIN 16
#define LED_S_NODE_PIN 17
#define LED_S_BATT_PIN 5
#define LED_POUT_PIN 18
#define LED_STAT_PIN 2
#define LED_STAT2_PIN 4

class LEDsInternal : public LEDs
{
    public:
        LEDsInternal(LEDCallbackFunc Callback, bool DisableThread);

        void AddScript(uint8_t ID, const uint8_t *data, uint16_t len);
        void StartScript(uint16_t ID, bool TempOverride);
        void StopScript(uint16_t ID);

        void SetGlobalVariable(uint8_t ID, float Value);
        void SetLocalVariable(uint16_t ID, uint8_t VarID, float Value);
        float GetLEDValue(LEDEnum LED);
        void SetLEDValue(LEDEnum LED, float Value, uint32_t LengthMS);
        uint16_t GetAmbientSensor();

        void SetLEDBrightness(float BrightnessPercent);
        float GetLEDBrightness();
        void SetLEDCap(uint8_t Count);

        void Run();

    private:
        typedef struct LEDInfoStruct
        {
            uint16_t Channel;
            uint16_t OverrideValue;
            unsigned long OverrideTime;
            uint16_t CurrentVal;
            bool Enabled;
        } LEDInfoStruct;

        typedef struct ScriptLEDInfoStruct
        {
            uint16_t StartVal;
            uint16_t CurrentVal;
            uint16_t EndVal;
            uint16_t Time;              //how long the animation should take
            unsigned long StartTime;
        } ScriptLEDInfoStruct;

        typedef struct ScriptInfoStruct
        {
            uint8_t ID;
            const uint8_t *data;
            uint16_t len;
            uint16_t active_pos;
            unsigned long DelayEndTime;
            uint8_t StopSet;
            bool active;
            bool TempOverride;
            uint16_t Variable[15];
            uint16_t LEDMask;
            ScriptLEDInfoStruct leds[LED_Count];            
            ScriptInfoStruct *stackfirst;
            ScriptInfoStruct *stackprev;
            ScriptInfoStruct *stacknext;
            ScriptInfoStruct *next;
        } ScriptInfoStruct;

        void set_led_pin(LEDEnum led, uint8_t pin, uint8_t ledChannel);
        uint16_t GetLevel(ScriptInfoStruct *cur_script, uint8_t Command);
        void SetLED(ScriptInfoStruct *cur_script, uint8_t entry);
        void ReadAmbientSensor();
        void AddToStack(ScriptInfoStruct *script);
        void RemoveFromStack(ScriptInfoStruct *script);
        ScriptInfoStruct *FindScriptForLED(uint16_t LED);

        ScriptInfoStruct *scripts;
        LEDInfoStruct leds[LED_Count];
        pthread_t LEDThread;
        uint16_t GlobalVariables[16];
        LEDCallbackFunc Callback;
        uint16_t AmbientSensorValue;
        unsigned long LastAmbientReading;
        uint16_t MaxBrightness;
        float MaxBrightnessPercent;
        uint8_t LEDCap;
};

#endif
