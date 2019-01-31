#include "smBut.h"

smBut::smBut() {}
smBut::~smBut() {}
/*
 smBut::smBut(int p, int m) {
  if (m==0) pinMode(p, INPUT); 
  if (m==1) pinMode(p, INPUT_PULLUP); mode=m; btPin=p;
 }
*/   
 
  void smBut::button(input in) {
   switch (in) {
    case input::Release:
      
      //Serial.println("Release");
      btState=state::Idle;
      switch (btState) {
        case state::Click: 
          
          break;
        case state::Hold: 
        
          //Serial.println("offLHold");
          break;
        case state::LongHold: 
          
          //Serial.println("offLong"); 
          break;
        case state::ForcedIdle: 
         
          break;
      }
      break;
    case input::WaitDebounce:    
      switch (btState) {
        case state::PreClick:
          //Serial.println("Click");
          btState=state::Click;
        break;
      }
      break;
    case input::WaitHold:       
      switch (btState) {
        case state::Click:
         // Serial.println("Hold");
          btState=state::Hold;
         break;
      }
      break;
    case input::WaitLongHold:      
      switch (btState) {
        case state::Hold:
         // Serial.println("Long");
          btState=state::LongHold;
         break;
      }
      break;
    case input::WaitIdle:       
      switch (btState) {
        case state::LongHold:
         // Serial.println("LongLong");
         // btState=state::ForcedIdle;
          break;
      }
      break;
    case input::Press: flag_on=1;
      switch (btState) {
        case state::Idle:
         // Serial.println("Press");
          preEnter=millis();
          btState=state::PreClick;
          break;
      }
      break;
  }
}
                  
    byte smBut::start () {  
    unsigned long mls=millis();   
    if (digitalRead(btPin)!=mode)  {
      button(input::Press); postEnter=millis(); 
                                }   
    else {
      button(input::Release);  if (mls - postEnter > sm_waitHold)  {} 
         }
          
        if (mls - preEnter > sm_waitDebounce) button(input::WaitDebounce);
        if (mls - preEnter > sm_waitHold)     button(input::WaitHold);
        if (mls - preEnter > sm_waitLong)     button(input::WaitLongHold);
        if (mls - preEnter > sm_waitLongLong) button(input::WaitIdle);      
       
         
     return byte(btState);
  }



