/*
 * boi.h
 *
*/
// Don't import yourself twice
#ifndef __boi_h__
#define __boi_h__

#include <Arduino.h>
#include <Adafruit_INA219.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <Preferences.h>

#include "app.h"

#define BTN_PWR_PIN 12
#define BTN_ACT_PIN 19
#define BTN_DEFLOCK_PIN T4
#define BTN_BONUS_PIN 0

#define LED_AMB_PIN 2 // Ambient Light Sensor

#define BACKPOWER_OUT_PIN 15 // MOSFET for enabling backpower

#define VGAT_DIV_PIN 39
#define VBAT_DIV_PIN 36

#define RDY_4056_PIN 34
#define CHRG_4056_PIN 35

// Initialize VREF variables
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

//can't nest this as we can't forward declare it for messages.h if it is in the class per C++ spec
//the website expects each entry to be 4 bytes in size (int/uint32_t)
typedef struct SensorDataStruct
{
  int current;
  float bat_voltage_detected;
  float gat_voltage_detected;
  int ready_pin_detected;
  int charge_pin_detected;
  float shunt_voltage;
  float bus_voltage;
  float vbat_max;
  float vbat_min;
  float joules_average;
  float joules;
} SensorDataStruct;

class boi // Battery of Internet class
{
  float total_current_used;

  public:
    // Respond if any buttons were pressed, set trigger
    enum Buttons
    {
      BTN_PWR,
      BTN_ACT,
      DEFLOCK_BTN,
      BTN_BONUS,
      BTN_Count
    };

    enum CHGPinEnum
    {
      CHG_RDY,
      CHG_CHG,
      CHG_Count
    };

    boi();

    // Sense current should return a float value
    float sense_current();
    float sense_battery_level();

    int read_ambient_sensor();
    int read_current();

    // get all sensor data
    void get_sensor_data(SensorDataStruct *Sensor);
    void print_sensor_data();
    
    // Check for current across VCC and ground pins, set switch
    void toggle_backpower();

    // Check to see if we need to change our mode from button press, trigger on previous data
    bool button_pressed(Buttons button);
    uint32_t button_held(Buttons button);

    int get_chg_status();
    void set_charging_status();

    float adc_vref_vbat();
    float adc_vref_vgat();
    void calibrate_capacity_measure(float vbat);

    unsigned int fetchResetCounter();

    //get total and an average over 10 seconds
    void get_joules(float *total_joules, float *average_joules, float watts);

  // private:
  private:
    Adafruit_INA219 ina219;

    float vbat_max_mv;
    float vbat_min_mv;
    float total_joules;
    float average_joules;
    uint32_t TotalMSForAverageJoules;

    uint8_t ButtonPins[BTN_Count];
    unsigned long ButtonState[BTN_Count];
    uint8_t CHGPins[CHG_Count];
    short full_vbat_mv = 1100;
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characteristics_t adc_chars_gat;
    const adc_channel_t channel = ADC_CHANNEL_0;     //GPIO34 if ADC1, GPIO14 if ADC2
    const adc_channel_t channel_gat = ADC_CHANNEL_3;     //GPIO34 if ADC1, GPIO14 if ADC2
    const adc_atten_t atten = ADC_ATTEN_DB_0;
    const adc_unit_t unit = ADC_UNIT_1;
    Preferences preferences;
    int boot_count;
    SensorDataStruct LastSensorData;
    unsigned long LastSensorDataUpdate;
    unsigned long LastJoulesSaveTime;

    void get_charging_status(SensorDataStruct *Data);
    void set_button_pin(Buttons button, uint8_t pin);
    void set_chg_pin(CHGPinEnum input, uint8_t pin);

    void initPreferences();
};

#endif
