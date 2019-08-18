// Author: Aask
// Creation Date: 05/04/2019
//
// Preface: This is the main library for the DEF-CELL: WiFi Internet of Batteries for DEF CON 27
// This file contains everything needed to run the basic functions on the DEF CELL, as well as
// some other fun tools.

#include "app.h"
#include <string>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "boi/boi_wifi.h"
#include "boi/boi.h"
#include "messages.h"
#include "leds.h"

// Operating frequency of ESP32 set to 80MHz from default 240Hz in platformio.ini

// Initialize Global variables
float current_sensed; // This should be read out of Flash each boot, but not yet :)

typedef enum ModeEnum
{
  BatteryMode = 0,
  NodeCountMode,
  PartyMode,
  Mode_Count
} ModeEnum;

uint8_t CurLED = 0; // Keep track of what LED is lit
bool SafeBoot = 0;
uint8_t CurrentMode = 0;
uint8_t ModeSelectedLED[Mode_Count] = {LED_TWINKLE, LED_DEFAULT_ALL, LED_BLINKLE};
uint8_t PrevModeSelectedLED;
uint8_t TouchCount = 0;
uint8_t SelectingLED = 0;
uint8_t LastNodeCount = 0;
uint8_t LEDCap = 5;

// Create global boi class
boi *Battery;
boi_wifi *BatteryWifi;
Messages *MessageHandler;
LEDs *LEDHandler;
Preferences *Prefs;

unsigned long LastTouchCheckTime = 0;
unsigned long LastTouchSetTime = 0;
unsigned long LastSensorPrintTime = 0;
unsigned long LastButtonPressTime = 0;
unsigned int LastButtonPwrHeldTime = 0;
unsigned int LastButtonActHeldTime = 0;
unsigned int LastButtonBonusHeldTime = 0;
unsigned long LastScanTime = 0;
uint8_t CurrentLEDCap = 0;

void SwitchMode();
float safeboot_voltage();

void LEDCallback(const uint16_t ID, bool Finished)
{
  //if a script finished then see if it matches the ID of the mode we are in
  //if so then restart it
  if(Finished)
  {
    //if the selected script indicates it completed then make it restart
    if(ModeSelectedLED[CurrentMode] == ID)
      LEDHandler->StartScript(ID, 0);
  }
}

void setup(void) {
  uint8_t WifiState;
  float BatteryPower;

  // init system debug output
  Serial.begin(115200);
  Serial.println("Beginning BoI Setup!");

  BatteryPower = safeboot_voltage();

  //if the action pin is held then go into safe mode
  pinMode(BTN_ACT_PIN, INPUT_PULLUP);
  pinMode(BTN_PWR_PIN, INPUT_PULLUP);
  if((digitalRead(BTN_ACT_PIN) == 0) || (BatteryPower < 3.7))
  {
    SafeBoot = 1;
    LEDHandler = NewLEDs(LEDCallback, true);
    Serial.println("Safe boot, reboot to disable");

    if(BatteryPower < 3.7)
      Serial.println("Battery power too low");

    //action button is held, check and see if power button is held
    //if so then we want to do a factory reset
    if(digitalRead(BTN_PWR_PIN) == 0)
    {
      //cycle LED_100 every second indicating we are getting ready to do a factory reset
      for(int i = 5; i > 0; i--)
      {
        Serial.printf("Doing factory reset in %d second(s), reset to avoid\n", i);
        LEDHandler->SetLEDValue(LED_LEV100, 100.0, LED_OVERRIDE_INFINITE);
        delay(500);
        LEDHandler->SetLEDValue(LED_LEV100, 0.0, LED_OVERRIDE_INFINITE);
        delay(500);
      }

      Serial.println("Factory reset activated");

      //turn on LED100 to indicate doing reset
      LEDHandler->SetLEDValue(LED_LEV100, 100.0, LED_OVERRIDE_INFINITE);
      MessageHandler = NewMessagesHandler();
      MessageHandler->DoFactoryReset();

      //reset done, half power the led to indicate complete
      LEDHandler->SetLEDValue(LED_LEV100, 50.0, LED_OVERRIDE_INFINITE);
    }

    return;
  }

  Prefs = new Preferences();

  LEDHandler = NewLEDs(LEDCallback, false);
  LEDCap = 5;
  LEDHandler->SetLEDCap(LEDCap);

  //this is on purpose as it calls the AddScript function for us
  #include "leds-setup.h"

  //setup params
  Prefs->begin("options");
  Prefs->getBytes("lightshows", ModeSelectedLED, sizeof(ModeSelectedLED));
  WifiState = Prefs->getUChar("wifi", boi_wifi::NormalMode);
  Prefs->end();

  // initialize boi
  Battery = new boi();

  // Current Sensing Operation - Check to see if we have anything on the pins 
  Serial.println("Current Sensed:"+String(Battery->read_current()));
  LastTouchCheckTime = esp_timer_get_time();
  LastSensorPrintTime = LastTouchCheckTime;

  MessageHandler = NewMessagesHandler();

  BatteryWifi = 0;

  //go to our mode that is set
  SwitchMode();

  //if we aren't configured or wifi was on then turn on the wifi
  if(!MessageHandler->GetOptions()->Configured || WifiState)
  {
    Serial.printf("WifiState: %d\n", WifiState);
    BatteryWifi = new boi_wifi(Battery, MessageHandler, (boi_wifi::WifiModeEnum)WifiState);
  }
}

float safeboot_voltage()
{
  esp_adc_cal_characteristics_t adc_chars;

  //Configure ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten((adc1_channel_t)ADC_CHANNEL_0, ADC_ATTEN_DB_0);

  //Characterize ADC
  memset(&adc_chars, 0, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

  //Sample ADC Channel
  uint32_t adc_reading = 0;
  //Multisampling
  for (int i = 0; i < NO_OF_SAMPLES; i++) {
    adc_reading += adc1_get_raw((adc1_channel_t)ADC_CHANNEL_0);
  }
  adc_reading /= NO_OF_SAMPLES;
  // Read voltage and then use voltage divider equations to figure out actual voltage read
  uint32_t v_out = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);//divide by two per 
  float R1 = 1000000.0;
  float R2 = 249000.0;
  float v_source = ((v_out * R1/R2) + v_out)/1000;

  return v_source;
}

void loop() {
  SensorDataStruct SensorData;
  uint32_t HeldTime;
  int8_t NewLEDCap;

  if(SafeBoot)
  {
    Serial.printf("In safeboot - battery voltage: %fV\n", safeboot_voltage());
    esp_sleep_enable_timer_wakeup(2000000);
    esp_light_sleep_start();
    return;
  }

  //get sensor data
  Battery->get_sensor_data(&SensorData);
  
  //if our voltage is not 0 then see if the led's need to be updated
  if(SensorData.bat_voltage_detected > 0.0)
  {
    //adjust our light limit based on the sensor data
    float VoltagePerStep = (3.9 - 3.6) / 5;

    //calculate it
    NewLEDCap = 5 - ((3.9 - SensorData.bat_voltage_detected) / VoltagePerStep);
    if(NewLEDCap > 5)
      NewLEDCap = 5; 
    else if(NewLEDCap < 0)
      NewLEDCap = 0;

    //set the led limit based on battery voltage
    if((NewLEDCap != CurrentLEDCap) && (CurrentMode == BatteryMode))
    {
      Serial.printf("Voltage: %0.3f, cap: %d\n", SensorData.bat_voltage_detected, NewLEDCap);
      LEDHandler->SetLEDCap(NewLEDCap);
      CurrentLEDCap = NewLEDCap;
    }
  }
    
  // print sensor data every 3 seconds
  unsigned long CurTime = esp_timer_get_time();
  if(((CurTime - LastSensorPrintTime) / 1000000ULL) >= 3) {
    LastSensorPrintTime = CurTime;
    Battery->print_sensor_data();
  }

  // Check to see if a button was pressed or other event triggered
  uint8_t DoublePress_ACT = 0;
  if(Battery->button_pressed(boi::BTN_ACT))
  {
    Serial.println("Saw button BTN_ACT");

    //determine if this was a double click
    if((CurTime - LastButtonPressTime) <= 400000ULL)
      DoublePress_ACT = 1;
    else
      LastButtonPressTime = CurTime;
  }

  //if ACTION button double pressed then swap the captive portal otherwise swap led options
  if(DoublePress_ACT)
  {      
    if(BatteryWifi)
    {
      delete BatteryWifi;
      BatteryWifi = 0;

      //indicate wifi is off    
      Prefs->begin("options");
      Prefs->putBool("wifi", false);
      Prefs->end();
    }
    else
    {
        BatteryWifi = new boi_wifi(Battery, MessageHandler, boi_wifi::NormalMode);
    }
    LastButtonPressTime = 0;
  }
  else if(LastButtonPressTime && ((CurTime - LastButtonPressTime) > 500000ULL)) {
    //button was pressed in the past and was more than a half a second ago, do a light entry
    LastButtonPressTime = 0;
    
    // Only trigger on the first time around
    CurrentMode = (CurrentMode + 1) % 3;
    Serial.printf("BTN_ACT pressed, mode %d\n", CurrentMode);
    TouchCount = 0;
    SelectingLED = 0;

    //flash briefly that we are selecting a mode
    SwitchMode();
  }
  else if(Battery->button_pressed(boi::BTN_PWR)){
      
    Serial.println("Toggling Backpower if possible!");

    // Attempt to toggle backpower
    Battery->toggle_backpower();
  }
  else if(Battery->button_pressed(boi::BTN_BONUS))
  {
    // Only trigger on the first time around
    Serial.println("BONUS button pressed!");

    LastButtonPressTime = 0;
  }

  //check for LED brightness increase
  if(MessageHandler->GetOptions()->BrightnessButtons)
  {
    float BrightnessPercent;
    HeldTime = Battery->button_held(boi::BTN_ACT);
    if(HeldTime)
    {
      if(!LastButtonActHeldTime)
        LastButtonActHeldTime = HeldTime;
      else
      {
        //action button is held, increment brightness as it is held
        //increment every 100ms
        if((HeldTime - LastButtonActHeldTime) >= 100)
        {
          LastButtonActHeldTime = HeldTime;
          BrightnessPercent = LEDHandler->GetLEDBrightness();
          BrightnessPercent += 0.01;
          LEDHandler->SetLEDBrightness(BrightnessPercent);
        }
      }
    }
    else
    {
      //if we had a time and it is greater than 100ms we need to save the new option value
      if(LastButtonActHeldTime && ((HeldTime - LastButtonActHeldTime) >= 100))
        MessageHandler->BrightnessModified();
        
      LastButtonActHeldTime = 0;
    }

    //check for LED brightness decrease
    HeldTime = Battery->button_held(boi::BTN_PWR);
    if(HeldTime)
    {
      if(!LastButtonPwrHeldTime)
        LastButtonPwrHeldTime = HeldTime;
      else
      {
        //action button is held, increment brightness as it is held
        //increment every 100ms
        if((HeldTime - LastButtonPwrHeldTime) >= 100)
        {
          LastButtonPwrHeldTime = HeldTime;
          BrightnessPercent = LEDHandler->GetLEDBrightness();
          BrightnessPercent -= 0.01;
          LEDHandler->SetLEDBrightness(BrightnessPercent);
        }
      }
    }
    else
    {
      //if we had a time and it is greater than 100ms we need to save the new option value
      if(LastButtonPwrHeldTime && ((HeldTime - LastButtonPwrHeldTime) >= 100))
        MessageHandler->BrightnessModified();

      LastButtonPwrHeldTime = 0;
    }
  }

  //check for bonus button held
  HeldTime = Battery->button_held(boi::BTN_BONUS);
  if(HeldTime)
  {
    if(!LastButtonBonusHeldTime)
    {
      LastButtonBonusHeldTime = HeldTime;

    //trigger action for bonus held
    LEDHandler->StartScript(LED_CLASSIC_BONUS, 1);
    }
  }
  else
  {
    if(LastButtonBonusHeldTime){
      LEDHandler->StopScript(LED_CLASSIC_BONUS);

      LastButtonBonusHeldTime = 0;
    }

  }

  //check for the touch sensor every half a second
  if((CurTime - LastTouchCheckTime) > 500000ULL)
  {
    LastTouchCheckTime = CurTime;
    byte touch = touchRead(BTN_DEFLOCK_PIN);

    if(touch < 52)
    {
      //if held for 3 seconds then indicate we are selecting a led mode
      if(TouchCount >= 6)
      {
        //flash briefly that we are setting the default for this mode
        if(!SelectingLED)
        {
          SelectingLED = 1;
          LEDHandler->SetLEDCap(5);
          PrevModeSelectedLED = ModeSelectedLED[CurrentMode];
        }
      }
      else
        TouchCount++;

      //even if the value is reset, if we are still in select mode then set it
      if(SelectingLED)
      {
        //stop the old script and start the new one
        Serial.printf("Selecting new LED for mode %d\n", CurrentMode);
        LEDHandler->StopScript(ModeSelectedLED[CurrentMode]);
        ModeSelectedLED[CurrentMode] = (ModeSelectedLED[CurrentMode] + 1) % LED_SCRIPT_COUNT;
        LEDHandler->StartScript(ModeSelectedLED[CurrentMode], 0);
        LastTouchSetTime = CurTime;

        //if the first time then do our flash animation on LED 100
        if(SelectingLED == 1)
        {
          LEDHandler->StartScript(LED_LED_100_FLASH, 1);
          SelectingLED = 2;
        }
      }
    }else{
      TouchCount = 0;

      //if it's been 10 seconds flash and save the setting
      if(SelectingLED && (CurTime - LastTouchSetTime) >= 10000000)
      {
        SelectingLED = 0;
        LastTouchSetTime = 0;

        //flash briefly that we are setting the default for this mode
        LEDHandler->StartScript(LED_LED_100_FLASH, 1);

        //save the value
        Prefs->begin("options");
        Prefs->putBytes("lightshows", ModeSelectedLED, sizeof(ModeSelectedLED));
        Prefs->end();
        
        //re-establish our cap as it might have changed depending on the mode
        LEDHandler->SetLEDCap(LEDCap);
      }
    }
  }

  //if we are in nodecount or wifi is active then do a scan every 30 seconds
  if((CurrentMode == NodeCountMode) || BatteryWifi)
  {
    if((CurTime - LastScanTime) >= 30000000ULL)
    {
      LastScanTime = CurTime;
      MessageHandler->DoScan();
    }
  }
  //if our mode is NodeCount then see if we have a new count to set
  if(CurrentMode == NodeCountMode)
  {
    uint8_t CurNodeCount = MessageHandler->GetPingCount();
    if(LastNodeCount != CurNodeCount)
    {
      LastNodeCount = CurNodeCount;
      if(CurNodeCount > 5)
        CurNodeCount = 5;

      //set cap then update if not selecting a lightshow
      LEDCap = CurNodeCount;
      if(!SelectingLED)
        LEDHandler->SetLEDCap(CurNodeCount);
    }
  }

  yield();
  delay(10);
}

void SwitchMode()
{
  //if they were in the middle of selecting an led then reset it
  if(SelectingLED)
  {
    SelectingLED = 0;
    ModeSelectedLED[CurrentMode] = PrevModeSelectedLED;
  }

  switch(CurrentMode)
  {
    case BatteryMode:   //battery charge mode
      //stop party mode
      LEDHandler->StopScript(LED_PARTY_MODE);

      //stop the previously selected script and start ours
      LEDHandler->StopScript(ModeSelectedLED[Mode_Count - 1]);
      LEDHandler->StartScript(ModeSelectedLED[BatteryMode], 0);

      //set a high time so it will take a long time to expire
      LEDHandler->SetLEDValue(LED_S_BATT, 100.0, 0xffffffff);
      LEDHandler->SetLEDValue(LED_S_NODE, 0.0, 0xffffffff);
      break;

    case NodeCountMode:   //node count mode
      //stop the previously selected script and start ours
      LEDHandler->StopScript(ModeSelectedLED[NodeCountMode - 1]);
      LEDHandler->StartScript(ModeSelectedLED[NodeCountMode], 0);

      //set a high time so it will take a long time to expire
      LEDHandler->SetLEDValue(LED_S_BATT, 0.0, 0xffffffff);
      LEDHandler->SetLEDValue(LED_S_NODE, 100.0, 0xffffffff);

      //kick off a scan
      LEDCap = 0;
      LEDHandler->SetLEDCap(LEDCap);
      MessageHandler->DoScan();
      break;

    case PartyMode:   //party mode
      //quickly set the values to 0 with no time so it expires
      LEDHandler->SetLEDValue(LED_S_BATT, 0.0, 0);
      LEDHandler->SetLEDValue(LED_S_NODE, 0.0, 0);

      //stop the previously selected script and start ours
      LEDHandler->StopScript(ModeSelectedLED[PartyMode - 1]);
      LEDHandler->StartScript(ModeSelectedLED[PartyMode], 0);

      //start the bounce led script for it
      LEDHandler->StartScript(LED_PARTY_MODE, 1);

      //set the cap to 5 for party mode
      LEDCap = 5;
      LEDHandler->SetLEDCap(LEDCap);

      break;
  }
}