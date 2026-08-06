#include <ESP32Servo.h>

Servo esc;
const int escPin = 14;
const int throttleUs = 1300; //super low throttle, tune this to check spin

void setup() {
  esc.setPeriodHertz(50);
  esc.attach(escPin, 1000, 2000);
  esc.writeMicroseconds(1000);  //arm
  delay(5000);                 //check spin

  esc.writeMicroseconds(throttleUs); 
}

void loop() {

}