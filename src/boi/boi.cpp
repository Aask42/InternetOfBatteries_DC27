/*
 *
 */

#include "boi.h"
#include "app.h"
#include <math.h>
#include <esp_wifi.h>
#include <string>
#include <Wire.h>


// Initialize ledc variables
int backpower_status = 1;
int backpower_status_auto_off = 0;
int curr_curr = 0;
int prev_curr = 0;
float start_bright_percent;    
float bright_duty_percent;
bool charging = 0;
bool charged = 0;
pthread_mutex_t status_lock;
u_long last_tick_time = esp_timer_get_time();

float boi::adc_vref_vbat(){  
    //Configure ADC
    if (this->unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten((adc1_channel_t)this->channel, this->atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)this->channel, this->atten);
    }

    //Characterize ADC
    memset(&this->adc_chars, 0, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(this->unit, this->atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, &this->adc_chars);

    //Sample ADC Channel
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        if (unit == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)this->channel);
        } else {
            int raw;
            adc2_get_raw((adc2_channel_t)this->channel, ADC_WIDTH_BIT_12, &raw);
            adc_reading += raw;
        }
    }
    adc_reading /= NO_OF_SAMPLES;
    // Read voltage and then use voltage divider equations to figure out actual voltage read
    uint32_t v_out = esp_adc_cal_raw_to_voltage(adc_reading, &this->adc_chars);//divide by two per 
    float R1 = 1000000.0;
    float R2 = 249000.0;
    float v_source = ((v_out * R1/R2) + v_out)/1000;

    return v_source;
}
float boi::adc_vref_vgat(){
  
    //Configure ADC
    if (this->unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten((adc1_channel_t)this->channel_gat, this->atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)this->channel_gat, this->atten);
    }

    //Characterize ADC
    memset(&this->adc_chars_gat, 0, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(this->unit, this->atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, &this->adc_chars_gat);

    //Sample ADC Channel
    uint32_t adc_reading_gat = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        if (unit == ADC_UNIT_1) {
            adc_reading_gat += adc1_get_raw((adc1_channel_t)this->channel_gat);
        } else {
            int raw;
            adc2_get_raw((adc2_channel_t)this->channel_gat, ADC_WIDTH_BIT_12, &raw);
            adc_reading_gat += raw;
        }
    }
    adc_reading_gat /= NO_OF_SAMPLES;

    // Read voltage and then use voltage divider equations to figure out actual voltage read
    uint32_t v_out = esp_adc_cal_raw_to_voltage(adc_reading_gat, &this->adc_chars_gat);
    float R1 = 1000000.0;
    float R2 = 249000.0;
    float v_source = ((v_out * R1/R2) + v_out)/1000;

    return v_source;
}

void boi::get_joules(float *total_joules, float *average_joules, float watts){
    // J = W * s
    float joules_consumed_in_ms_since_update;
    u_long current_tick_time = esp_timer_get_time();

    //get milliseconds passed
    u_long ms_passed = (current_tick_time - last_tick_time) / 1000;
    if(!ms_passed)
    {
        //not a full millisecond since last check, just return last value
        *total_joules = this->total_joules;
        *average_joules = this->average_joules;
        return;
    }

    joules_consumed_in_ms_since_update = (watts * ms_passed) / 1000.0;

    //we now have how many have been consumed since last tick, add it to our total
    this->total_joules += joules_consumed_in_ms_since_update;
    *total_joules = this->total_joules;

    //figure out an average over 5 seconds to return

    //average_joules stores our value for 1ms of time
    //so figure out how many total joules were used in that time window
    float total_joules_per_ms = this->average_joules * this->TotalMSForAverageJoules;

    //now add in our consumed amount and how may ms it took
    total_joules_per_ms += joules_consumed_in_ms_since_update;
    this->TotalMSForAverageJoules += ms_passed;

    //if our total MS is > 5 seconds then remove from the average a proper amount before the
    //new average is calculated
    if(this->TotalMSForAverageJoules > 5000)
    {
        total_joules_per_ms -= (this->average_joules * (this->TotalMSForAverageJoules - 5000));
        this->TotalMSForAverageJoules = 5000;
    }

    //calculate a new average per ms and return how many that is for 5s
    this->average_joules = total_joules_per_ms / this->TotalMSForAverageJoules;
    *average_joules = this->average_joules * 5000;

    //Serial.printf("Joules consumed this round: %f\n",joules_consumed);
    //Serial.printf("Joules consumed since last factory reset: %f\n",joules);

    last_tick_time = current_tick_time;

    //if 5 seconds have passed then update the flash so we dont loose it if the device reboots
    if((current_tick_time - LastJoulesSaveTime) > 5000000ULL)
    {
        this->preferences.begin("boi");
        this->preferences.putFloat("total_joules", this->total_joules);
        this->preferences.end();

        LastJoulesSaveTime = current_tick_time;
    }
}

boi::boi(){
    Serial.println("Initializing boi class...");

    memset(this->ButtonPins, 0, sizeof(this->ButtonPins));
    memset(this->ButtonState, 0, sizeof(this->ButtonState));
    memset(this->CHGPins, 0, sizeof(this->CHGPins));
    this->vbat_max_mv = 0;
    this->vbat_min_mv = 0;
    this->LastSensorDataUpdate = 0;
    this->TotalMSForAverageJoules = 0;
    this->average_joules = 0;
    this->LastJoulesSaveTime = 0;
    memset(&this->LastSensorData, 0, sizeof(SensorDataStruct));

    pthread_mutex_init(&status_lock, NULL);

    this->ina219.begin();
    this->ina219.setCalibration_16V_500mA();
    this->initPreferences();

    // Button assignment
    this->set_button_pin(boi::BTN_PWR, BTN_PWR_PIN);
    this->set_button_pin(boi::BTN_ACT, BTN_ACT_PIN);
    this->set_button_pin(boi::BTN_BONUS, BTN_BONUS_PIN);

    pinMode(BACKPOWER_OUT_PIN, OUTPUT); // Set up BACKPOWER_OUT_PIN, defaults to "on"
    
    this->toggle_backpower();        // Toggle backpower off

    pinMode(VGAT_DIV_PIN,INPUT_PULLUP);        
    pinMode(VBAT_DIV_PIN,INPUT_PULLUP);
    pinMode(RDY_4056_PIN,INPUT);
    pinMode(CHRG_4056_PIN,INPUT);
}

int boi::read_current(){

    int current = ina219.getCurrent_mA();
    
    if(prev_curr != curr_curr){
        prev_curr = curr_curr;
        curr_curr = current;
        int x = (curr_curr - prev_curr);
        Serial.printf("Change of Current being drawn: %d\n", x);
    }

    return current;
}

// Dump all stats from the battery
void boi::get_sensor_data(SensorDataStruct *Sensor){
    //if we got stats recently then just return them to avoid excessive hitting
    if((this->LastSensorDataUpdate + 150000ULL) > esp_timer_get_time())
    {
        memcpy(Sensor, &this->LastSensorData, sizeof(SensorDataStruct));
        return;
    }

    //attempt to lock the mutex
    if(pthread_mutex_trylock(&status_lock))
    {
        //we failed to lock, someone else snatched it, just return the original data
        memcpy(Sensor, &this->LastSensorData, sizeof(SensorDataStruct));
        return;
    }

    //force update so we can get new data
    this->LastSensorDataUpdate = esp_timer_get_time();

    //go get our values
    Sensor->bat_voltage_detected = this->adc_vref_vbat();
    this->calibrate_capacity_measure(Sensor->bat_voltage_detected);
    Sensor->gat_voltage_detected = this->adc_vref_vgat();
    yield();

    Sensor->ready_pin_detected = digitalRead(RDY_4056_PIN);
    Sensor->charge_pin_detected = digitalRead(CHRG_4056_PIN);
    this->get_charging_status(Sensor);

    Sensor->current = this->read_current();
    
    this->get_joules(&Sensor->joules, &Sensor->joules_average, Sensor->current);

    Sensor->shunt_voltage = ina219.getShuntVoltage_mV()/20.0;
    if((Sensor->shunt_voltage < -0.25) && backpower_status){
        Serial.print("Power detected going the wrong way! Disbling Backpower!!!");
        this->toggle_backpower();
        backpower_status_auto_off = 1;
    }
    if((Sensor->shunt_voltage >= 0) && backpower_status_auto_off){
        Serial.print("Coast is clear! Re-enable backpower!");
        this->toggle_backpower();
        backpower_status_auto_off = 0;
    }
    yield();
    Sensor->bus_voltage = ina219.getBusVoltage_V();

    Sensor->vbat_max = this->vbat_max_mv;
    Sensor->vbat_min = this->vbat_min_mv;

    //copy our data and update our last time
    memcpy(&this->LastSensorData, Sensor, sizeof(SensorDataStruct));

    pthread_mutex_unlock(&status_lock);
}

void boi::print_sensor_data()
{
    SensorDataStruct Data;
    this->get_sensor_data(&Data);
    
    Serial.print("=====InternetOfBatteries=====\n\n");
    Serial.print("By: Aask, Lightning, and true\n\n");
    Serial.printf(" ""\xE2\x96\xB2\xC2\xA0""=%s= ""\xC2\xA0\xE2\x96\xB2""\n", MessageHandler->GetOptions()->WifiName);
    Serial.print("""\xE2\x96\xB2""\xC2\xA0""\xE2\x96\xB2""===""\xC2\xAF\x20\x5C\x20\x5F\x20\x28\xE3\x83\x84\x29\x20\x5F\x20\x2F\x20\xC2\xAF""===""\xE2\x96\xB2""\xC2\xA0""\xE2\x96\xB2""\n");
    Serial.printf("Current Current Detected: %dmW\n", Data.current);
    Serial.printf("BUS Voltage Detected: %0.2f\n", Data.bus_voltage);
    Serial.printf("SHUNT Voltage Detected: %02.fV\n", Data.shunt_voltage);
    Serial.printf("VBAT Voltage Detected: %0.2fV u\n",  Data.bat_voltage_detected);
    Serial.printf("VBAT Min/Max: %0.2f / %0.2f\n", Data.vbat_min, Data.vbat_max);
    Serial.printf("VGAT Voltage Detected: %0.2fV\n", Data.gat_voltage_detected);
    Serial.printf("Average Joules consumed in 5 seconds: %0.2fμJ\n", Data.joules_average);
    Serial.printf("Joules consumed since factory reset: %0.2fμJ\n", Data.joules);
    //Ret += "Ambient Voltage Detected: "+String(ambient_voltage_detected)+"\n");
    Serial.printf("Battery RDY PIN Detected: %d\n", Data.ready_pin_detected);
    Serial.printf("Battery CHRG PIN Detected: %d\n", Data.charge_pin_detected);
    Serial.printf("Total number of reboots: %d\n", this->boot_count);
    Serial.print("  ""\xE2\x96\xB2""\xC2\xA0\xC2\xA0""@BatteryInternet""""\xC2\xA0\xC2\xA0\xE2\x96\xB2""\n");
    Serial.print("""\xC2\xA0\xE2\x96\xB2""\xC2\xA0""\xE2\x96\xB2""==""==============""==""\xE2\x96\xB2""\xC2\xA0""\xE2\x96\xB2""""\n\n");
}

// Check for current across VCC and ground pins, set switch
void boi::toggle_backpower(){
    
    int current_detected = ina219.getCurrent_mA();

    // Check current power-draw through INA219, print if above zero
    if(current_detected > 5.00){
        Serial.println("Current Sensed:"+String(current_detected) + "mAh");
    }

    Serial.println("Toggling backpower...: ");
    Serial.println("Initial Backpower Status: "+String(backpower_status));

    switch(backpower_status){
        case 0:
            digitalWrite(BACKPOWER_OUT_PIN, 0);
            Serial.println("BACKPOWER_OUT_PIN is now engaged!: "+ String(digitalRead(BACKPOWER_OUT_PIN)));
            Serial.println("-. ..- -- -... . .-. _/̄ˉ ..... _/̄ˉ .. ... _/̄ˉ .- .-.. .. ...- . -.-.-- -.-.--");
            backpower_status = 1;

            // Enable backpower ON LED
            LEDHandler->StartScript(LED_POUT_ON, 1);
            Serial.println("Backpower Enabled");
            
            break;
        case 1:
            Serial.println("Disabling backpower on pin "+String(BACKPOWER_OUT_PIN)+"!");     
            Serial.println("-. --- _/̄ˉ -.. .. ... .- ... ... . -- -... .-.. . -.-.-- -.-.-- -.-.--");
            digitalWrite(BACKPOWER_OUT_PIN, 1);
            Serial.println("BACKPOWER_OUT_PIN has been disengaged! : "+ String(digitalRead(BACKPOWER_OUT_PIN)));
            backpower_status = 0;

            // Disble backpower ON LED
            LEDHandler->StartScript(LED_POUT_OFF, 1);
            Serial.println("Disabled backpower!");
            break;
    };
}

// Check to see if a button was pressed
bool boi::button_pressed(Buttons button)
{
    bool ret;
    int btnState;
    btnState = !digitalRead(this->ButtonPins[button]);

    //if the button is not held and was held in the past then indicate it was pressed
    //we go based off a 5ms cycle to avoid situations where power glitches cause the button to randomly trip
    //we also limit to half a second so that pressed never reports if held is being triggered
    unsigned long CurTime = esp_timer_get_time();
    if(!btnState && this->ButtonState[button] && ((CurTime - this->ButtonState[button]) > 5000) && ((CurTime - this->ButtonState[button]) < 500000))
        ret = true;
    else
        ret = false;

    //if button is not held then set value to 0
    //if button is pressed but we don't have a value then set the value
    if(!btnState)
        this->ButtonState[button] = 0;
    else if(!this->ButtonState[button])
        this->ButtonState[button] = CurTime;

    return ret;
}

//indicate how many milliseconds a button is held
uint32_t boi::button_held(Buttons button)
{
    uint32_t ret;
    int btnState;
    btnState = !digitalRead(this->ButtonPins[button]);

    //if the button is held and was held in the past then indicate how long
    unsigned long CurTime = esp_timer_get_time();
    if(btnState && this->ButtonState[button] && ((CurTime - this->ButtonState[button]) >= 500000))
        ret = (CurTime - this->ButtonState[button]) / 1000ULL;
    else
        ret = 0;

    //if button is not held then set value to 0
    //if button is pressed but we don't have a value then set the value
    if(!btnState)
        this->ButtonState[button] = 0;
    else if(!this->ButtonState[button])
        this->ButtonState[button] = CurTime;

    return ret;
}

void boi::set_button_pin(Buttons button, uint8_t pin)
{
    this->ButtonPins[button] = pin;
    pinMode(pin, INPUT_PULLUP);
}

void boi::set_chg_pin(CHGPinEnum input, uint8_t pin)
{
    this->CHGPins[input] = pin;
}

void boi::initPreferences(){
    this->preferences.begin("boi");
    this->boot_count = this->preferences.getUInt("counter", 0) + 1;
    this->preferences.putUInt("counter", this->boot_count);
    this->total_joules = this->preferences.getFloat("total_joules", 0.0);
    this->vbat_max_mv = this->preferences.getFloat("vbat_max", 0);
    this->vbat_min_mv = this->preferences.getFloat("vbat_min", 0);
    this->preferences.end();

    Serial.printf("Battery has been booted %d times ^_^\n", boot_count);
}

void boi::calibrate_capacity_measure(float vbat){
     if((vbat > this->vbat_max_mv) || (this->vbat_max_mv == 0)){
        this->preferences.begin("boi");
        this->preferences.putFloat("vbat_max", vbat);
        this->preferences.end();
        this->vbat_max_mv = vbat;
    }
    if((vbat < this->vbat_min_mv) || (this->vbat_min_mv == 0)){
        this->preferences.begin("boi");
        this->preferences.putFloat("vbat_min", vbat);
        this->preferences.end();
        this->vbat_min_mv = vbat;
    }
    this->preferences.end();
}

void boi::get_charging_status(SensorDataStruct *Data){
    int charge_status = Data->charge_pin_detected;
    int ready_status = Data->ready_pin_detected;
    if(charge_status == 0 && ready_status == 1){
        if(charging == 0){
            Serial.print("Charging! ");
            LEDHandler->StartScript(LED_CHARGING, 1);
            charging = 1;
        }
    }else if(charge_status == 0 && ready_status == 0){
        if(charged == 0){
            Serial.print("Charged!!!");
            LEDHandler->StopScript(LED_CHARGING);
            charged = 1;
        }
    }else{
        if(charging == 1){
            LEDHandler->StopScript(LED_CHARGING);
            charged = 0;
            charging = 0;
        }
    }
}