#ifndef SM_BUT_H
#define SM_BUT_H

#if ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif


#ifndef sm_waitDebounce
#define sm_waitDebounce 20
#endif
#ifndef sm_waitHold
#define sm_waitHold     250
#endif
#ifndef sm_waitLong
#define sm_waitLong     850
#endif
#ifndef sm_waitLongLong
#define sm_waitLongLong 2500
#endif

class smBut {
  
  public:
   int btPin, butSt;
   smBut();
   smBut(int p, int m) {
    if (m==0) pinMode(p, INPUT); 
    if (m==1) pinMode(p, INPUT_PULLUP); mode=m; btPin=p;
   }
   ~smBut();
   byte start ();

  private:
    unsigned long preEnter, postEnter;
    bool flagOff, flag_on;
    int  mode;
    enum class state : byte  {Idle, PreClick, Click, Hold, LongHold, ForcedIdle};
    enum class input : byte  {Release, WaitDebounce, WaitHold, WaitLongHold, WaitIdle, Press};
    state btState;
    void button (input in);

};
#endif
