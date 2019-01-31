#include <smBut.h>
smBut bt1(7,1);

void setup() {
  Serial.begin(9600);
  pinMode(13, OUTPUT);
}

void loop() {
  
  if(bt1.start()==4 ) digitalWrite(13, 1); else digitalWrite(13, 0);
  
}
