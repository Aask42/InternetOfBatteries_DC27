General ToDo
----
- [ ] Scoreboard
- [ ] X party/business card need to turn on wifi

Aask todo
---------
- [ ] bonus button show capacity - Just needs capacity limit now....
- [ ] store max time device has been seen running, stored every minute, displayed on website
- [ ] enter to send doesn't work

Not required but nice to have
-----------------------------
- [ ] tune twinkle a little
- [ ] Do ambient light sensor

webpage
-------
- [ ] Message box wider on the screen

To test
-------

Done
-----
- [x] disconnect from other badge needs to fail faster when other side fails to respond
- [x] wifi needs stays on after reconfigured name and reboot, only off if was off on reboot
- [x] Modify LED script code handler to handle limited LEDs for scripts that rely on state (example wifi off script)
----
- [x] Undo factory reset commented out code
- [x] auto turn wifi off after x amount of time of inactivity (no user interaction)
- [x] Battery power level
- [x] Auto shutoff when power is low, go into safe mode - battery range, 4.2v to 3.6v, auto off at 3.7
- [x] Need to still be able to reset connection if other side still thinks you are connected
    connect badges  
    reset badge 1  
    disconnect, see new connection established  
    reset badge 1  
    disconnect, new connection does not establish  
    unable to disconnect with future attempts  
- [x] Able to force disconnect private chat
- [x] can't disconnect if conn reset, can after message
- [x] scan page doesn't auto refresh
- [x] Nickname changes do not appear to show real time
- [x]     Make sure new nickname is seen upon re-connection established
- [x] Store sent private messages into flash for reload later
- [x]     Modify private message storage to indicate to/from for load later
- [x] graffiti is broke, won't show up on other side nor allow user past it
- [x] if wifi is on then flash an led
- [x] red status indicator should flash when notification for user, initial boot should flash slowly
- [x] Store sent global messages into global message list
- [x] Store some portion of group messages in flash, maybe every minute or so to avoid excessive flash hits?
- [x] Modify private message storage to not be wiped out upon connection to portal, continue loop cycle
- [x] text box doesn't clear when a message is sent
- [x] global chat is not being stored for access in the system if the wifi was off
- [x] add flash stored flag of wifi state so wifi will go back to a proper state if rebooted/crashed
- [x] brightness slider doesn't work in windows when not configured yet
- [x] apply on config settings isn't showing a succeed message at top of screen on windows
- [x] fix scripts affecting pout to leave light on after script finishes
- [x] turning on wifi a 2nd time temp freezes led code, potentially all chip processing, why only after the first power up/down?
- [x] Factory reset implemented
- [x] load and store lightshows as they are set
- [x] Graffiti window not activating
- [x] Confirm nickname changes show up
- [x] back power notification does not turn on when back power turns on
- [x] shorten double press time   700ms -> 500ms
- [x] swap over to led_20/40/60 system scripts for turning led on/off on mode swap
- [x] show node count when in node mode
- [x] Check how long double click takes and set timeout accordingly
- [x] force reboot upon wifi name or password change
- [x] not redirected when going to different websites
- [x] Set default LED scripts for battery/node/party modes
- [x] make touch button more sensitive
- [x] wifi on/off animations missing
- [x] Force full firmware reset if both power and action held on power on
- [x] charging indicators on website backwards - unplugged / charging / charged instead of showing seperately
- [x] Limit max led based off of remaining capacity for all LED scripts
- [x] Fix showing full LEDs for events but not normal function
- [x] websocket needs periotic ping to stay alive
- [x] wifi always on for first boot until configured
- [x] orange more copper color on webpage
- [x] modify html in scan area to alert them to click a name to connect for private chat
- [x] put username and wifi name in status area
- [x] Brightness adjust from button press should be toggle in configuration
