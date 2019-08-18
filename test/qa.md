# This is a checklist of features to validate on the Internet of Batteries

## Automated States

- [x] Charging
    - [x] Animation
    - [x] Implemented in code
    - [x] Trigger
    - [x] Web browser notification

- [ ] Charged
    - [x] Animation
    - [x] Implemented in code
    - [x] Trigger
    - [ ] Actually working? Questionable.....

- [ ] Private Message Received (3x Wifi Off)
    - [ ] Animation
    - [X] Implemented in code
    - [ ] Notification
    - [x] Trigger

- [ ] Public Message Received (1x WiFi Off)
    - [ ] Animation
    - [X] Implemented in code
    - [ ] Notification
    - [x] Trigger
  
- [ ] Master Message Received (5x Wifi Off)
    - [ ] Animation
    - [X] Implemented in code
    - [ ] Notification
    - [x] Trigger
  
- [ ] Private Message Sent (3x WiFi On)
    - [ ] Animation
    - [X] Implemented in code
    - [ ] Notification
    - [x] Trigger

- [ ] Public Message Sent (1x WiFi On)
    - [ ] Animation
    - [X] Implemented in code
    - [ ] Notification
    - [x] Trigger

- [ ] Master Message Sent (5x Wifi On)
    - [ ] Animation
    - [ ] Implemented in code
    - [ ] Notification
    - [ ] Trigger

- [ ] New Score Received
    - [ ] Animation
    - [ ] Implemented in code
    - [ ] Notification
    - [ ] Trigger

- [x] Graffiti
    - [X] Store in flash
    - [X] Read from flash
    - [X] Received from connection
    - [x] Send to connection
    - [x] Display to user
    - [x] Received from user for a connection

## User Set States
- [X] Options
    - [X] Set led brightness
    
- [x] Enable Wifi(Same as Receive Message)
    - [x] Animation
    - [X] Implemented in code
    - [X] Double Press One to Enable

- [x] Disable Wifi
    - [x] Animation
    - [X] Implemented in code
    - [X] Double press One to Disable
        - [x] Fix bug where reboots on disable
    - [x] Disables after timeout

-[x] Node count
    - [x] Animation
    - [x] Implemented in code
    - [x] Display accurate node count of batteries in the area

- [ ] Bonus Buttion
    - [x] Animation
    - [x] Implemented in code
    - [x] Press & hold BONUS BTN to trigger animation of battery capacity
    - [ ] At end of hold, switch to business card broadcasting "I'M A BUSINESS CARD!!" with business card emojis

- [ ] Party
    - [x] Animation (Twinkle Light Show)
    - [x] Cycle Light Shows by pressing DEF LOCK button for 500ms
    - [ ] Broadcast "I'M A BATTERY MORTY!" + html for original IoB
    - [x] Selected Light show is set as default
    - [x] Implemented in code

- [ ] Enable Backpower
    - [ ] Animation
    - [X] Implemented in code
    - [X] Single Press 2

- [ ] Disable Backpower
    - [ ] Animation
    - [X] Implemented in code
    - [X] Single Press 2

- [x] Auto-Disable Backpower
    - [x] Animation
    - [x] Implemented in code

## User Interface

- [X] Send PM to ONE boi
- [x] Send PM to a SECOND boi
- [X] Receive PM to ONE boi
- [x] Receive PM to a SECOND boi
- [X] Send Group Broadcast
- [X] Receive Group Broadcast
- [ ] Configure Options
    -[X] Wifi Network Name
        - [X] Defaults to what is set in Preferences
        - [ ] If custom, original battery name is still stored in memory for user to swap in, default name button
    - [X] Wifi Network Password
    - [x] Max LEDs based on remaining battery vs user choice
- [x] Write Grafitti on TWO bois from ONE boi
- [x] View Grafitti on ONE boi from TWO bois
- [ ] View score board current standings

## Odds and ends

- [x] Get a cake
- [ ] Get MORE cake
- [ ] Make some challenges
- [x] Auto power off on low battery level
- [x] Auto safe mode on low battery
- [x] Calibration function for INA219 with 200ohm resistor
- [x] Auto back power off if shunt resistor current direction changes
- [ ] Ambient light sensor on boot
- [X] User configurable brightness in options menu
~~- [ ] Quit while we're ahead~~

https://github.com/me-no-dev/ESPAsyncWebServer/issues/394