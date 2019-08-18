/*
 * app.h
 *
*/
// Don't import yourself twice
#ifndef __app_h__
#define __app_h__
#include <Arduino.h>
#include "boi/boi.h" // Class library
#include "boi/boi_wifi.h"
#include "leds.h"

class boi;
class boi_wifi;
class LEDs;
class Messages;
extern boi_wifi *BatteryWifi;
extern LEDs *LEDHandler;
extern Messages *MessageHandler;

#endif