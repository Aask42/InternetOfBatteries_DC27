#include "leds_internal.h"
#include <Preferences.h>

LEDsInternal *_globalLEDs;
pthread_mutex_t led_lock;

#define FREQUENCY 5000
#define BIT_RESOLUTION 15
#define MAX_RESOLUTION (1 << BIT_RESOLUTION)
#define POWER_LAW_A 0.4

LEDs *NewLEDs(LEDCallbackFunc Callback, bool DisableThread)
{
    LEDsInternal *LED = new LEDsInternal(Callback, DisableThread);
    return LED;
}
void *static_run_leds(void *)
{
    if(_globalLEDs)
        _globalLEDs->Run();

    return 0;
}

LEDsInternal::LEDsInternal(LEDCallbackFunc Callback, bool DisableThread)
{
    ScriptInfoStruct *script;

    //initialize
    this->scripts = 0;
    this->Callback = Callback;

    memset(this->leds, 0, sizeof(this->leds));

    // LED assignment of pin and ledChannel
    this->set_led_pin(LED_LEV20, LED_LEV20_PIN, 0);
    this->set_led_pin(LED_LEV40, LED_LEV40_PIN, 1);
    this->set_led_pin(LED_LEV60, LED_LEV60_PIN, 2);
    this->set_led_pin(LED_LEV80, LED_LEV80_PIN, 3);
    this->set_led_pin(LED_LEV100, LED_LEV100_PIN, 4);
    this->set_led_pin(LED_S_NODE, LED_S_NODE_PIN, 5);
    this->set_led_pin(LED_S_BATT, LED_S_BATT_PIN, 6);
    this->set_led_pin(LED_POUT, LED_POUT_PIN, 7);
    this->set_led_pin(LED_STAT, LED_STAT_PIN, 8);
    this->set_led_pin(LED_STAT2, LED_STAT2_PIN, 9);

    //allocate a default led setup
    script = (ScriptInfoStruct *)malloc(sizeof(ScriptInfoStruct));
    memset(script, 0, sizeof(ScriptInfoStruct));

    //setup LED_STAT2 pin so that we can use STAT properly
    ledcWrite(this->leds[LED_STAT2].Channel, MAX_RESOLUTION); // - 1000);

    //set a default ID and assign to the script list
    script->ID = 0xff;
    this->scripts = script;

    //now set the brightness default, this will force STAT and STAT2 to be updated too
    script->active = 1;
    this->SetLEDBrightness(0.09);
    script->active = 0;
    
    //wipe out the global variables
    memset(this->GlobalVariables, 0, sizeof(this->GlobalVariables));
    this->AmbientSensorValue = 0.0;
    this->LastAmbientReading = 0;

    _globalLEDs = this;

    //if we are to disable threads then exit
    if(DisableThread)
        return;

    //setup our thread that will run the LEDs themselves
    pthread_mutex_init(&led_lock, NULL);
    if(pthread_create(&this->LEDThread, NULL, static_run_leds, 0))
    {
        //failed
        Serial.println("Failed to setup LED thread");
        _globalLEDs = 0;
        return;
    }
}

void LEDsInternal::set_led_pin(LEDEnum led, uint8_t pin, uint8_t ledChannel)
{
    this->leds[led].Channel = ledChannel;

    pinMode(pin, OUTPUT);
    ledcSetup(this->leds[led].Channel, FREQUENCY, BIT_RESOLUTION);
    ledcAttachPin(pin, this->leds[led].Channel);
    this->leds[led].Enabled = 1;

    //turn the LED off, MAX results in full off, 0 is full bright
    ledcWrite(this->leds[led].Channel, MAX_RESOLUTION);
}

uint16_t LEDsInternal::GetLevel(ScriptInfoStruct *cur_script, uint8_t Command)
{
    uint16_t Val1;
    uint16_t Val2;
    uint16_t Range;
    uint16_t *data16 = (uint16_t *)&cur_script->data[cur_script->active_pos];

    //pass in a command allowing a check for a random value to exist
    if(Command & 0xE0)
    {
        //just get the value specified
        Val1 = *data16;
        data16++;
        Val2 = *data16;

        //move the data pointer forward by 2 entries
        cur_script->active_pos += sizeof(uint16_t) * 2;

        //get a random value between val1 and val2 inclusive
        Range = Val2 - Val1 + 1;
        if(Range)
        {
            Val2 = esp_random();
            Val1 = (Val2 % Range) + Val1;
        }
    }
    else
    {
        //just get the value specified
        Val1 = *data16;

        //move the data pointer forward
        cur_script->active_pos += sizeof(uint16_t);

        //if the value is 0xfff0 to 0xfffe then we need to get a local variable
        //if the value is between 0xffe0 and 0xffef then we need to get a global variable
        //0xffff is reserved for commands like STOP to indicate no value
        if(Val1 >= 0xffe0 && (Val1 <= 0xffef))
            Val1 = this->GlobalVariables[Val1 & 0x0f];
        else if((Val1 >= 0xfff0 && (Val1 <= 0xfffe)))
            Val1 = cur_script->Variable[Val1 & 0x0f];
    }
    
    //return the value
    return Val1;
}

void LEDsInternal::Run()
{
    uint8_t Command;
    uint8_t LogicSeen;
    uint8_t i;
    uint8_t CurrentLED;
    uint16_t TempPos;
    uint16_t LEDsUpdated;

    ScriptInfoStruct *cur_script;

    Serial.println("LED thread running\n");
    while(1)
    {
        //lock the mutex so that we avoid anyone modifying the data while we need it
        pthread_mutex_lock(&led_lock);

        //execute each script that is active
        cur_script = this->scripts;
        if(!cur_script)
        {
            //not done setting scripts up
            pthread_mutex_unlock(&led_lock);
            delay(500);
            continue;
        }

        while(cur_script)
        {
            //if not active skip it
            if(!cur_script->active)
            {
                cur_script = cur_script->next;
                continue;
            }

            //execute all instructions until we hit an instruction to wait on
            //if we are not currently delayed
            LogicSeen = 0;
            LEDsUpdated = 0;
            while(!cur_script->DelayEndTime && !LogicSeen)
            {
                Command = cur_script->data[cur_script->active_pos];
                cur_script->active_pos++;

                //top 2 bits indicate random selections exist in the data
                switch(Command & 0x1F)
                {
                    case 0: //set
                    {
                        CurrentLED = cur_script->data[cur_script->active_pos];
                        cur_script->active_pos++;
                        cur_script->leds[CurrentLED].EndVal = this->GetLevel(cur_script, Command);
                        if(cur_script->leds[CurrentLED].EndVal == 0xffff)
                            cur_script->leds[CurrentLED].EndVal = cur_script->leds[CurrentLED].CurrentVal;

                        cur_script->leds[CurrentLED].StartVal = cur_script->leds[CurrentLED].EndVal;
                        cur_script->leds[CurrentLED].Time = 0;
                        LEDsUpdated |= (1 << CurrentLED);
                        break;
                    }

                    case 1: //add
                    {
                        CurrentLED = cur_script->data[cur_script->active_pos];
                        cur_script->active_pos++;
                        uint16_t NewLevel = this->GetLevel(cur_script, Command);
                        if(NewLevel == 0xffff)
                            NewLevel = 0;
                        cur_script->leds[CurrentLED].EndVal = cur_script->leds[CurrentLED].CurrentVal + NewLevel;

                        //cap at 100%
                        if(cur_script->leds[CurrentLED].EndVal >= 10000)
                            cur_script->leds[CurrentLED].EndVal = 10000;

                        cur_script->leds[CurrentLED].StartVal = cur_script->leds[CurrentLED].EndVal;
                        cur_script->leds[CurrentLED].Time = 0;
                        LEDsUpdated |= (1 << CurrentLED);
                        break;
                    }

                    case 2: //sub
                    {
                        CurrentLED = cur_script->data[cur_script->active_pos];
                        cur_script->active_pos++;
                        uint16_t NewLevel = this->GetLevel(cur_script, Command);
                        if(NewLevel == 0xffff)
                            NewLevel = 0;
                        cur_script->leds[CurrentLED].EndVal = cur_script->leds[CurrentLED].CurrentVal + NewLevel;

                        //cap at 0%, it is unsigned so it will wrap around
                        if(cur_script->leds[CurrentLED].EndVal >= 10000)
                            cur_script->leds[CurrentLED].EndVal = 0;

                        cur_script->leds[CurrentLED].StartVal = cur_script->leds[CurrentLED].EndVal;
                        cur_script->leds[CurrentLED].Time = 0;
                        LEDsUpdated |= (1 << CurrentLED);
                        break;
                    }
                    
                    case 3: //move
                    {
                        CurrentLED = cur_script->data[cur_script->active_pos];
                        cur_script->active_pos++;
                        cur_script->leds[CurrentLED].StartVal = this->GetLevel(cur_script, Command & 0x8F);
                        cur_script->leds[CurrentLED].EndVal = this->GetLevel(cur_script, Command & 0x4F);
                        cur_script->leds[CurrentLED].Time = this->GetLevel(cur_script, Command & 0x2F);
                        cur_script->leds[CurrentLED].StartTime = esp_timer_get_time() / 1000;

                        //if either start or end is a * then set it to current
                        if(cur_script->leds[CurrentLED].StartVal == 0xffff)
                            cur_script->leds[CurrentLED].StartVal = cur_script->leds[CurrentLED].CurrentVal;
                        if(cur_script->leds[CurrentLED].EndVal == 0xffff)
                            cur_script->leds[CurrentLED].EndVal = cur_script->leds[CurrentLED].CurrentVal;
                        LEDsUpdated |= (1 << CurrentLED);
                        break;
                    }

                    case 0x10:  //delay
                    {
                        uint32_t DelayAmount = this->GetLevel(cur_script, Command);
                        cur_script->DelayEndTime = esp_timer_get_time() + (DelayAmount * 1000);
                        LogicSeen = 1;
                        break;
                    }

                    case 0x11:   //stop
                    {
                        uint32_t DelayAmount = this->GetLevel(cur_script, Command);
                        if(DelayAmount == 0xffff)
                            cur_script->DelayEndTime = 0xffffffff;      //hard stop
                        else
                            cur_script->DelayEndTime = esp_timer_get_time() + (DelayAmount * 1000);
                        LogicSeen = 1;
                        cur_script->StopSet = 2;
                        break;
                    }

                    case 0x12:  //if
                    case 0x14:  //wait
                    case 0x15:  //ifled
                    {
                        TempPos = cur_script->active_pos;
                        CurrentLED = cur_script->data[cur_script->active_pos];
                        cur_script->active_pos++;

                        //if the LED being checked was modified in this cycle then wait a cycle to validate
                        if(LEDsUpdated & (1 << CurrentLED))
                        {
                            cur_script->active_pos = TempPos - 1;
                            LogicSeen = 1;
                            continue;
                        }

                        uint8_t MathType = cur_script->data[cur_script->active_pos];
                        cur_script->active_pos++;

                        //get the value
                        uint16_t CompareValue;
                        if((Command & 0x1F) == 0x15)
                        {
                            CompareValue = cur_script->leds[cur_script->data[cur_script->active_pos]].CurrentVal;
                            cur_script->active_pos++;
                        }
                        else
                            CompareValue = this->GetLevel(cur_script, Command);
                    
                        //compare them
                        uint8_t ValueMatched = 0;
                        switch(MathType)
                        {
                            case 0: //=
                                if(cur_script->leds[CurrentLED].CurrentVal == CompareValue)
                                    ValueMatched = 1;
                                break;

                            case 1: //<
                                if(cur_script->leds[CurrentLED].CurrentVal < CompareValue)
                                    ValueMatched = 1;
                                break;

                            case 2: //>
                                if(cur_script->leds[CurrentLED].CurrentVal > CompareValue)
                                    ValueMatched = 1;
                                break;

                            case 3: //<=
                                if(cur_script->leds[CurrentLED].CurrentVal <= CompareValue)
                                    ValueMatched = 1;
                                break;

                            case 4: //>=
                                if(cur_script->leds[CurrentLED].CurrentVal >= CompareValue)
                                    ValueMatched = 1;
                                break;
                        };

                        //if we match then branch to the proper location otherwise just advance
                        if(ValueMatched)
                        {
                            //if, not wait then follow the label
                            if((Command & 0x1F) == 0x12)
                                cur_script->active_pos = this->GetLevel(cur_script, 0);
                        }
                        else if((Command & 0x1F) == 0x14)
                        {
                            //wait failed, go back to beginning of wait
                            cur_script->active_pos = TempPos - 1;
                        }

                        //indicate we saw logic
                        LogicSeen = 1;
                        break;
                    }

                    case 0x13:  //goto
                    {
                        //change the active position
                        cur_script->active_pos = this->GetLevel(cur_script, Command);
                        LogicSeen = 1;
                        break;
                    }
                };

                //if the active position is ahead then set it to 0 and indicate we are done
                if(cur_script->active_pos >= cur_script->len)
                {
                    //first 2 bytes are led bit mask
                    cur_script->active_pos = 2;
                    LogicSeen = 1;

                    //if we have a callback then let go of the mutex incase they try to call start/stop
                    //note, we also need to make sure we aren't in a final stop state as we are reporting loop here
                    if(this->Callback && (!cur_script->StopSet || (cur_script->DelayEndTime != 0xffffffff)))
                    {
                        pthread_mutex_unlock(&led_lock);
                        this->Callback(cur_script->ID, false);
                        pthread_mutex_lock(&led_lock);
                    }                    
                }
            }

            //adjust our delay end time
            if(esp_timer_get_time() >= cur_script->DelayEndTime)
            {
                cur_script->DelayEndTime = 0;
                cur_script->StopSet = 0;
            }

            //if stop is set then delay shortly and loop around
            if(cur_script->StopSet)
            {
                //if this is 1 then delay otherwise allow a single cycle through so any updates can occur
                if(cur_script->StopSet == 1)
                {
                    //if delay time is infinite then turn the script off
                    if(cur_script->DelayEndTime == 0xffffffff)
                    {
                        //deactivate the script
                        cur_script->active = 0;

                        pthread_mutex_unlock(&led_lock);
                        this->StopScript(cur_script->ID);

                        //handle callback indicating done                        
                        if(this->Callback)
                            this->Callback(cur_script->ID, true);

                        pthread_mutex_lock(&led_lock);                        
                    }
                    continue;
                }

                //subtract 1 so we can do a single cycle on the values
                cur_script->StopSet--;
            }

            //next script
            yield();
            cur_script = cur_script->next;
        };

        pthread_mutex_unlock(&led_lock);

        //done executing instructions, go through each LED and update it's status
        cur_script = 0;
        for(i = 0; i < LED_Count; i++)
        {
            float Gap;
            float PerStepIncrement;
            unsigned long CurrentTime = esp_timer_get_time() / 1000;
            unsigned long LapsedTime;

            //see if we need to adjust over time
            if(this->leds[i].OverrideTime)
            {
                //we are in override, see if we need to turn it off
                if((this->leds[i].OverrideTime != LED_OVERRIDE_INFINITE) && (this->leds[i].OverrideTime <= CurrentTime))
                    this->leds[i].OverrideTime = 0;
            }
            else
            {
                //determine the active script impacting this led
                //we allow for multiple scripts to be active without using the same
                //led so re-check if the mask doesn't have the led
                if(!cur_script || !(cur_script->LEDMask & (1 << i)))
                    cur_script = this->FindScriptForLED(i);

                //if no script then continue
                if(!cur_script)
                    continue;

                if(cur_script->leds[i].Time)
                {
                    //calculate how far along we should be
                    Gap = (float)cur_script->leds[i].EndVal - (float)cur_script->leds[i].StartVal;
                    PerStepIncrement = Gap / (float)cur_script->leds[i].Time;
                    LapsedTime = CurrentTime - cur_script->leds[i].StartTime;
                    cur_script->leds[i].CurrentVal = cur_script->leds[i].StartVal + (PerStepIncrement * LapsedTime);

                    //if we hit the end then set the final value
                    if(LapsedTime >= cur_script->leds[i].Time)
                    {
                        cur_script->leds[i].Time = 0;
                        cur_script->leds[i].CurrentVal = cur_script->leds[i].EndVal;
                    }

                    this->SetLED(cur_script, i);
                }
                else if(cur_script->leds[i].CurrentVal != cur_script->leds[i].EndVal)
                {
                    //we just need to set it and go
                    cur_script->leds[i].CurrentVal = cur_script->leds[i].EndVal;
                    this->SetLED(cur_script, i);
                }
            }

            /*
            //if this is the STAT led then determine if we can read the ambient sensor passively or if we are forced to read it
            //due to being too long as we have to turn off pin 2
            if(i == LED_STAT)
            {
                //it is stat, see if the current value is low enough to turn off briefly
                unsigned long CurTime = esp_timer_get_time();

                //5 seconds since last read and light is 5% or lower, go read
                if(((this->LastAmbientReading + 5000000ULL) <= CurTime) && (cur_script->leds[i].CurrentVal <= 500))
                    this->ReadAmbientSensor();
                else if((this->LastAmbientReading + 15000000ULL) <= CurTime)
                    this->ReadAmbientSensor();   //over 15 seconds, force read
            }
            */
        }

        yield();
        delay(10);
    };
}

LEDsInternal::ScriptInfoStruct *LEDsInternal::FindScriptForLED(uint16_t LED)
{
    ScriptInfoStruct *cur_script = this->scripts;
    while(cur_script)
    {
        //if active and the mask bit is set then we want to use it
        if(cur_script->active && (cur_script->LEDMask & (1 << LED)))
        {
            //if we are part of a stack then grab the stack entry
            if(cur_script->stackfirst)
                cur_script = cur_script->stackfirst;

            //found a script impacting this led, stop looking
            return cur_script;
        }

        cur_script = cur_script->next;
    }

    //return that we couldn't find anything
    return 0;
}

void LEDsInternal::SetLED(ScriptInfoStruct *cur_script, uint8_t entry)
{
    //set the LED based on the current value
    //the value has to be flipped so 100% is MAX_BRIGHTNESS but starting from MAX_RESOLUTION
    //and moving towards 0 while 0% == MAX_RESOLUTION

    float NewBrightness;
    uint32_t FinalBrightness;
    uint16_t TempNewBrightness;

    //CurrentVal is 0 to 10000 decimal, convert the current value into a value between 0.0000 and 1.0000
    //for what percentage we want of MaxBrightness
    if(this->leds[entry].OverrideTime)
        TempNewBrightness = this->leds[entry].OverrideValue;
    else
    {
        if(!cur_script)
            return;

        TempNewBrightness = cur_script->leds[entry].CurrentVal;
    }

    NewBrightness = (float)TempNewBrightness / 10000.0;

    //if the STAT entry then swap the %
    if(entry == LED_STAT)
        NewBrightness = (1.0 - NewBrightness) * 2.0;

    //we need to scale NewBrightness per Stevens' power law, a should be between 0.33 and 0.5
    FinalBrightness = int(pow(pow(this->MaxBrightness, POWER_LAW_A) * NewBrightness, 1/POWER_LAW_A) + 0.5);
    
    //subtract new value from MAX_RESOLUTION to get the actual value to send
    FinalBrightness = MAX_RESOLUTION - FinalBrightness;

    //if enabled or override time is set then set the actual led
    if(this->leds[entry].Enabled || this->leds[entry].OverrideTime ||
        (cur_script && cur_script->TempOverride && (cur_script->LEDMask & (1 << entry))))
    {
        this->leds[entry].CurrentVal = TempNewBrightness;
        ledcWrite(this->leds[entry].Channel, FinalBrightness);
    }
}

void LEDsInternal::AddScript(uint8_t ID, const uint8_t *data, uint16_t len)
{
    Serial.printf("Adding script id %d\n", ID);

    ScriptInfoStruct *new_script = (ScriptInfoStruct *)malloc(sizeof(ScriptInfoStruct));
    memset(new_script, 0, sizeof(ScriptInfoStruct));

    //add to the list
    new_script->ID = ID;
    new_script->data = data;
    new_script->len = len;
    new_script->LEDMask = *(uint16_t *)data;

    new_script->next = this->scripts;
    this->scripts = new_script;
}

void LEDsInternal::StartScript(uint16_t ID, bool TempOverride)
{
    //swap out the script being ran
    ScriptInfoStruct *cur_script;

    cur_script = this->scripts;
    while(cur_script && (cur_script->ID != ID))
        cur_script = cur_script->next;

    if(!cur_script)
        return;

    Serial.printf("activating script %d\n", cur_script->ID);
    //start the script
    pthread_mutex_lock(&led_lock);
    cur_script->StopSet = 0;
    cur_script->DelayEndTime = 0;
    cur_script->active_pos = 2;     //skip led bit mask
    cur_script->active = 1;
    cur_script->TempOverride = TempOverride;
    this->AddToStack(cur_script);
    pthread_mutex_unlock(&led_lock);
}

void LEDsInternal::StopScript(uint16_t ID)
{
    //stop a specific script
    ScriptInfoStruct *cur_script;
    ScriptInfoStruct *active_script;
    int i;

    cur_script = this->scripts;
    if(ID == LED_ALL)
    {
        //stop all scripts
        Serial.println("Stopping all LED scripts");
        pthread_mutex_lock(&led_lock);
        while(cur_script)
        {
            cur_script->active = 0;
            cur_script->stackfirst = 0;
            cur_script->stackprev = 0;
            cur_script->stacknext = 0;
            cur_script->TempOverride = 0;
            cur_script = cur_script->next;
        };
        pthread_mutex_unlock(&led_lock);
    } 
    else
    {
        Serial.printf("Stopping LED script %d\n", ID);
        while(cur_script && (cur_script->ID != ID))
            cur_script = cur_script->next;

        pthread_mutex_lock(&led_lock);
        cur_script->active = 0;
        cur_script->TempOverride = 0;
        this->RemoveFromStack(cur_script);

        for(i = 0; i < LED_Count; i++)
        {
            //if this led was not being modified then skip it otherwise find it's new value
            if(!(cur_script->LEDMask & (1 << i)))
                continue;

            active_script = FindScriptForLED(i);

            if(active_script)
            {
                if(active_script->LEDMask & (1 << i))
                    this->SetLED(active_script, i);
            }
        }

        pthread_mutex_unlock(&led_lock);
    }
}

void LEDsInternal::AddToStack(ScriptInfoStruct *script)
{
    //grab our mask and look for any script that has a mask collision that is active
    ScriptInfoStruct *CurScript;
    ScriptInfoStruct *StackScript;
    ScriptInfoStruct *PrevStackScript;
    int i;

    //remove ourselves from any current chains just in-case
    this->RemoveFromStack(script);

    //set our current values based on what the LEDs currently have
    for(i = 0; i < LED_Count; i++)
        script->leds[i].CurrentVal = this->leds[i].CurrentVal;

    //now see if there is a chain to add to
    CurScript = this->scripts;
    while(CurScript)
    {
        //if not our current script, script is active, and it's mask collides then update it's stack
        if((CurScript != script) && CurScript->active && (CurScript->LEDMask & script->LEDMask))
        {
            //determine if we should be the head entry
            //this requires we either have the override flag set or the first entry does not
            //have override set
            if(script->TempOverride || (!CurScript->stackfirst || !CurScript->stackfirst->TempOverride))
            {
                //set ourselves as head of the stack
                script->stackfirst = script;
                script->stackprev = 0;

                //now our new script and we have a LED collision, grab the stack entry and walk
                //it updating the first for everyone in the list
                StackScript = CurScript->stackfirst;

                //if we have a first entry then tell it we are before it
                if(StackScript)
                {
                    StackScript->stackprev = script;
                    script->stacknext = StackScript;

                    //walk the stack and update the first for everyone
                    while(StackScript)
                    {
                        StackScript->stackfirst = script;
                        StackScript = StackScript->stacknext;
                    }
                }
                else
                {
                    //first chain entry
                    CurScript->stackfirst = script;
                    CurScript->stackprev = script;
                    script->stacknext = CurScript;
                }

                //set the script to pull valus from
                CurScript = script->stacknext;
            }
            else
            {
                //we can not the first entry, insert ourselves into the stack
                //we keep track of previous as we might have a stack of overrides
                StackScript = CurScript->stackfirst;
                PrevStackScript = CurScript;
                while(StackScript && StackScript->TempOverride)
                {
                    PrevStackScript = StackScript;
                    StackScript = StackScript->stacknext;
                };

                //update accordingly
                script->stackfirst = PrevStackScript->stackfirst;
                script->stacknext = StackScript;

                PrevStackScript->stacknext = script;
                if(StackScript)
                    StackScript->stackprev = script;

                //set the script to pull valus from
                CurScript = script->stackfirst;
            }

            //copy the current LED status to us, we just want the values
            //not any settings
            for(i = 0; i < LED_Count; i++)
            {
                script->leds[i].StartVal = CurScript->leds[i].CurrentVal;
                script->leds[i].EndVal = CurScript->leds[i].CurrentVal;
                script->leds[i].StartTime = 0;
                script->leds[i].Time = 0;
            }
            break;
        }

        //next one
        CurScript = CurScript->next;
    }
}

void LEDsInternal::RemoveFromStack(ScriptInfoStruct *script)
{
    //remove ourselves from a stack of scripts
    //if we are first then we need to update first for everyone
    ScriptInfoStruct *NewHead;
    ScriptInfoStruct *StackScript;

    //there is no stack, stop looking
    if(!script->stackfirst)
        return;

    //if we are the first entry then update the whole chain
    if(script->stackfirst == script)
    {
        NewHead = script->stacknext;
        NewHead->stackprev = 0;

        //if the new head does not have any entries then reset it's stackfirst
        if(!NewHead->stacknext)
            NewHead->stackfirst = 0;
        else
        {
            //update the chain for who is first
            StackScript = NewHead;
            while(StackScript)
            {
                StackScript->stackfirst = NewHead;
                StackScript = StackScript->stacknext;
            }
        }
    }
    else
    {
        //just remove from the chain
        if(script->stacknext)
            script->stacknext->stackprev = script->stackprev;
        if(script->stackprev)
            script->stackprev->stacknext = script->stacknext;

        //check the first entry, if the next entry is null due
        //to pointing at us prior then it is the last entry in
        //the chain so unset it's first entry
        if(!script->stackfirst->stacknext)
            script->stackfirst->stackfirst = 0;
    }

    //wipe out our prev/next for stack along with our first
    script->stackfirst = 0;
    script->stackprev = 0;
    script->stacknext = 0;
}

void LEDsInternal::SetGlobalVariable(uint8_t ID, float Value)
{
    this->GlobalVariables[ID] = Value * 100.0;
}

void LEDsInternal::SetLocalVariable(uint16_t ID, uint8_t VarID, float Value)
{
    ScriptInfoStruct *CurScript;
    CurScript = this->scripts;
    while(CurScript && CurScript->ID != ID)
        CurScript = CurScript->next;

    if(!CurScript)
        return;

    CurScript->Variable[VarID] = Value * 100.0;
}

float LEDsInternal::GetLEDValue(LEDEnum LED)
{
    return (float)this->leds[LED].CurrentVal / 100.0;
}

void LEDsInternal::SetLEDValue(LEDEnum LED, float Value, uint32_t LengthMS)
{
    //store the value and immediately update it
    this->leds[LED].OverrideValue = Value * 100.0;
    if(LengthMS == LED_OVERRIDE_INFINITE)
        this->leds[LED].OverrideTime = LED_OVERRIDE_INFINITE;
    else
        this->leds[LED].OverrideTime = (esp_timer_get_time() / 1000) + LengthMS;
    this->SetLED(0, LED);
}

uint16_t LEDsInternal::GetAmbientSensor()
{
    return this->AmbientSensorValue;
}

void LEDsInternal::ReadAmbientSensor()
{
    //store the last time we got the sensor
    this->LastAmbientReading = esp_timer_get_time();
    return;

    //this is all sorts of broke. We have to power down wifi and reset registers to fix this
}

void LEDsInternal::SetLEDBrightness(float BrightnessPercent)
{
    int i;

    //set the scale of the LED brightness, note that this needs to follow the same logic that SetLED does
    if(BrightnessPercent < 0.0)
        BrightnessPercent = 0.0;
    else if(BrightnessPercent > 1.0)
        BrightnessPercent = 1.0;

    this->MaxBrightnessPercent = BrightnessPercent;

    //calculate a new value for max
    this->MaxBrightness = int(pow(pow(MAX_RESOLUTION, POWER_LAW_A) * BrightnessPercent, 1/POWER_LAW_A) + 0.5);

    //force all LEDs to set their brightness accordingly
    for(i = 0; i < LEDEnum::LED_Count; i++)
        this->SetLED(this->FindScriptForLED(i), i);
}

float LEDsInternal::GetLEDBrightness()
{
    //return the percent set
    return this->MaxBrightnessPercent;
}

void LEDsInternal::SetLEDCap(uint8_t Count)
{
    uint8_t i;

    pthread_mutex_lock(&led_lock);

    //enable all LEDs
    for(i = LED_LEV20; i <= LED_LEV100; i++)
        this->leds[i].Enabled = 1;

    //make sure any LEDs above the count are off
    for(i = LED_LEV20 + Count; i <= LED_LEV100; i++)
    {
        this->leds[i].Enabled = 0;
        this->leds[i].OverrideValue = 0;
        this->leds[i].OverrideTime = 1;
    }

    //force all refreshed
    for(i = LED_LEV20; i <= LED_LEV100; i++)
        this->SetLED(this->FindScriptForLED(i), i);

    pthread_mutex_unlock(&led_lock);
}