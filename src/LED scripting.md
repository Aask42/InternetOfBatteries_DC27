# LED Scripting commands

Each command is on a single line, any line starting with // is a comment. Comments are not allowed on the same line as a command

## LED Definitions

Any commands with __LED__ allows one of the following values to indicate the LED

LED Name | LED Notes
-|-
LED_20 |  Bottom LED of the battery
LED_40 |
LED_60 |
LED_80 |
LED_100 | Top LED of the battery group
NODE | Node LED
BATT | Battery LED
STAT | Status LED
POUT | Power out LED

## LED Commands


These LED commands will set a state for a __LED__ and progress to the next line allowing multiple states to exist at the same time

Command | Output
-|-
__SET__ __LED__ _LEVEL_ | set a __LED__'s brightness to a specific _LEVEL_
__MOVE__ __LED__ _LEVEL1_ _LEVEL2_ _TIME_ | Move from _LEVEL1_ to _LEVEL2_ brightness over _TIME_
__ADD__ __LED__ _LEVEL_ | Add an amount to the __LED__ value
__SUB__ __LED__ _LEVEL_ | Subtract an amount from the __LED__ value

__If a command is from the following logic block of commands then further processing of the script will stop until the condition is met__

Command | Output
-|-
__DELAY__ _TIME_ | Delay for an amount of time before continuing. Does not stop current states from executing
__STOP__ _TIME_ | Stop for a certain amount of time, no states will be modified. If Time is missing then execution is done
__IF__ __LED__ _X_ _LEVEL_ _LABEL_ | Check a LED to be a value, if true then go to the specified label otherwise go to the immediate following command. Labels and comments are ignored when looking for the next command to execute. X is any math comparison, >, <, =, >=, <=
__WAIT__ __LED__ _X_ _LEVEL_ | Similar to IF, check a LED to be a value if true then progress to the next line otherwise wait on this line
__GOTO__ _LABEL_ | Goto a previously set label

__If the script gets to the end of it's commands without a STOP at the end then it will restart at the first line of the script__

## Useful Things To Know

- Commands are case insensitive.  
- Any _LEVEL_ value is from _0.00_ to _100.00_  
- A __*__ uses the value the __LED__ was at at the time the line started to be executed.
- If _LEVEL_ is __RAND__ then 2 more parameters will follow indicating a min/max range with which to generate a random value.  
    - The min/max is capped between _0.00_ and _100.00_.
- The scripts allow for variables to be updated from the code to impact the script. It supports local variables that only the script knows and global variables that all scripts share.
- If _LEVEL_ or _TIME_ is LVAR_X then the X can be between 1 and 15 to indicate a local script variable to look at
    - Note: _TIME_ values will be multiples of 100ms. Floating point values can be used to get portions, 1.5 will be interpretted as 150ms
- If _LEVEL_ or _TIME_ is GVAR_X then the X can be between 1 and 16 to indicate a global script variable to look at
    - Note: _TIME_ values will be multiples of 100ms. Floating point values can be used to get portions, 1.5 will be interpretted as 150ms
- _TIME_ is in milliseconds
- Any line with no spaces and ending with a __colon (:)__ is a label

## Programming
- Calling any entry is do-able by adding the line LEDHandler->(LED_X); where X is the uppercase version __filename__ with any spaces replaced with an underscore: **LEDHandler->(LED_TEST_FAST);**
- Calling StopScript with LED_ALL will stop all LED scripts otherwise the individual script specified will be stopped
- The SetLEDValue function does allow for a value greater than 100% allowing for brighter than max of any script running
- The two functions SetGlobalVariable and SetLocalVariable set variables accordingly. Local variables are only known to a specific script and globals are common to all
## Unrealized Dreams

Could extend this slightly and allow simple variables to be created allowing for more complex logic. Will eval implementing after getting
the base code done.