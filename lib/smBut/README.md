# smBut
    Ideas from https://github.com/nw-wind/SmartButton/ by "Sergey Keler"
It is a complete Arduino library for buttons having state pulled up to VCC(INPUT_PULLUP) or with high Z input (attach pullup resistor to negative).

You can redefine timings also.
// Debounce time where the button has been pressed but is not Click event.
#define sm_waitDebounce 20
// The hold time after pressing the button we have Hold event.
#define sm_waitHold     250
// The long hold time for LongHold event.
#define sm_waitLong     850
// The idle time after that the key is pressed too long time and become have no value as an error.
#define sm_waitLongLong 2500

Your_name_button.start(); return four state 0-not click, 1-click with debounce, 2-hold, 3-long hold and holds this state until the button is released.




