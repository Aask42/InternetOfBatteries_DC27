# Internet of Batteries: DEF CON 27 - "They don't support that"
__BoI | Definition: Noun, Battery of Internets__

Welcome to the Internet of Batteries: DEF CON 27 Edition! We've been told that the SAO standard "doesn't support" a battery back-powering the circuit on the VCC, which means we've done the most logical thing...Built a badge/SAO hybrid to support back-powering through it's pinout! While the badge is compatable with SAO 1.69bis, if plugged in to a DC27 badge it will most likely blow up the moon.

This project created a simple (and mostly-safe) battery badge which back/forward-powers any electronic badge & add-on via VCC pins, supplying a 500mA @ 3.3V while keeping track of how much power is drawn via backpowering and forward powering other badges and SAOs. This badge runs off of it's own power, and is able to operate as a standalone unit. To view power usage statistics users connect to the __Captive Arcade<sup>TM</sup>__ on the ESP32 to view local battery power consumption and discharge stats.

Additionally, there is a WiFi Mesh Network (Itero) which powers the IoB. Through this network you can send broadcast messages to one big group chat, or sent PMs to up to 25 added devices. To scan for available nodes in the area, add other batteries to your "friends" list, and interact with group & private chat, you will need to connect to the __Captive Arcade<sup>TM</sup>__. When connected, "Friends" and scanned nodes will show green. 

## Jump Start

1. Power switch ON to charge
2. If LEDs flash on boot, boi is in safe mode, charge it
3. BTN 1 on boot = force safe mode - faster charge
4. BTN 1 + BTN 2 on boot = factory reset
5. Orient VCC on headers
6. Drink all the booze
7. Hack all the things
8. See you 
@DEFCON 28!

## Better Instructions

- __Power switch must be in the ON position to charge__
- If the battery voltage is TOO LOW, the LEDS will flash ONCE on reboot, then go dark and output current battery voltage on serial out (115200). Once above 3.7v the battery should show animation while charging if rebooted at that time

There are FIVE buttons on the DC27 version of the Internet of Batteries: 

Button | Description | Actions
-|-|-
RESET | This button is located on the BACK of the DEF CELL, under the battery next to the power switch | This resets the Boi
BTN 1 | Cycle mode, 2x toggles __Captive Arcade<sup>TM</sup>__ | CLK 1 : Capacity -> Node Count -> Party!!! <br/><br/>DBL CLK 1 : Toggle Captive Arcade
BTN 2 | Toggles backpower on SAO rail on/off | Auto-off if voltage is detected going the wrong way across the SHUNT resistor
DEFLOCK | Stealth touch button where it says "DEF CON XXVII" in the white strip at the top of the battery. | Press and hold to cycle through and set light show for each mode
BONUS | Press and hold for a fun throwback, does not currently actually show capacity | boop booP boOP bOOP BOOP! __BOOP!__ __BOOP!__ __BOOP!__ 

## Boot Order

This is very important and will help to minimize blowing up like a Note in the sky

0. Detect power on rails: If voltage on LiPoly is too low, __FLASH LEDS ONCE__ and boot in to "Safe Mode"
1. Button assignment  : ```BTN_PWR, BTN_ACT```
1. LED assignment  : ```LED_LEV20, LED_LEV40, LED_LEV60, LED_LEV80, LED_LEV100, LED_S_NODE, LED_S_BATT, LED_POUT_ON```
1. DNS setup
2. WiFi setup
3. Enable __Captive Arcade<sup>TM</sup>__ if no password is set
4. React to user input
5. If voltage across shunt changes direction, and BACKPOWER is ON, force power state to NO_BACKPOWER

## Board Layout

Front:

```txt
      _____
 ____|ESP32|____ ---
|USB |     |PWR | |   
|STFF|_____|STFF| |   
|    SAO SAO    | |	
|USB            | | 
|    Battery    |~6.5cm
|    Goes       | |   
|    Here       | |  
|         INA219| |
|SAO_SWITCH__SAO| |
|_______________|---
|-----~3.5cm----|

Back: 
      _____
 ____|_____|____
|               |
| DEF CON XXIIV |
|    SAO SAO  D |
| BTN1 LED    E |
| L L  LED    F |
| E E  LED    C |
| D D  LED    E |
| BTN2 LED    L |
|     SHUNT   L |
|SAO_________SAO|
```

## How to flash new firmware

Note: We had to modify the libraries for the Adafruit INA219 and ASyncTCP in order to pull this off. Copies are provided in the source repo and should auto-import on build with PlatformIO

1. Install VS Code 
2. Install PlatformIO Extension
3. Clone this repository and open in VS Code
4. Plug Boi in to USB and identify the correct COM port if necessary (if only one COM device, should auto-select)
5. Test build code with PlatformIO (CTRL+SHIFT+B)
6. Upload code with PlatformIO (CTRL+SHIFT+U)

__Note:__ There are preferences stored in flash which persist even when flashing updated code. To reset ALL data on the device you must perform a full flash reset from PlatformIO, then flash the desired version of the firmware to the now empty device

## Donations

```
Bitcoin Cash is pretty sweet:  
    1CbJsCqH9btitkRr13m11RribyF6m7EUTZ

Bitcoin is cool too: 
    1GrnYwUCAsQY3oejdu8CRutnG55Wi7bXHz  

1 DOGE == 1 DOGE: 
    DNsRM5gPEA5MrGByi1MyWfbLWyuksvK5fC    
```