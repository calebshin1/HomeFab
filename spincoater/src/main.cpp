#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64   
#define OLED_RESET    -1   // ssd1306 has no reset pin
#define SCREEN_ADDRESS 0x3C 

//box button (PTS645) GPIO pins used on ESP32
#define OK 4
#define CANCEL 5
#define MINUS 18
#define PLUS 19

//RGB LED (5TB4URGB) GPIO pins used on ESP32
#define RED 25
#define GREEN 26
#define BLUE 27

//ESC sig pin and other relevant params
#define ESC_PIN 14
const int TEST_US = 1300; //from older RS2205 test harness
Servo esc;
struct spinParams{
  int targetRPM = 500;
  int duration = 10; //seconds
  unsigned long elasped = 0; //elapsed spin time
  int throttleUs = 1000; //throttle microseconds
};
spinParams motor;

enum spinPhase {ARM_WAIT, ARM_SEND, RUNNING};
spinPhase escPhase = ARM_WAIT;
unsigned long armStartUs = 0;
const unsigned long ARM_DELAY = 3000; 

//LED PWM params
const int freq = 5000;
const int res = 8;
const int redChl = 0;
const int greenChl = 1;
const int blueChl = 2;
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;

//LED color arrays (for easier organization)
const int whiteCode[3] = {60,60,60};
const int blueCode[3] = {0,0,90};
const int yellowCode[3] = {20,5,0};
const int greenCode[3] = {0,60,0};
const int offCode[3] = {0,0,0};

bool blink {}; //boolean for if blink mode or not
bool ledState {}; //for if led is on or not
unsigned long timeLastLED = 0; //value that tracks time between LED toggles
const unsigned long blinkDuration = 500; //ms

uint8_t state = 0; //default state is 0 which is idle
uint8_t timer = 0; //set and elapsed time, in seconds

Bounce2::Button ok = Bounce2::Button(); //setting up Bounce2 button objects
Bounce2::Button cancel = Bounce2::Button();
Bounce2::Button plus = Bounce2::Button();
Bounce2::Button minus = Bounce2::Button();



Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);

  //init each button via Bounce2 library
  ok.attach(OK, INPUT_PULLUP);
  ok.interval(5);
  ok.setPressedState(LOW);

  cancel.attach(CANCEL, INPUT_PULLUP);
  cancel.interval(5);
  cancel.setPressedState(LOW);

  plus.attach(PLUS, INPUT_PULLUP);
  plus.interval(5);
  plus.setPressedState(LOW);

  minus.attach(MINUS, INPUT_PULLUP);
  minus.interval(5);
  minus.setPressedState(LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true) delay(10); // halt
  }

  ledcSetup(redChl, freq, res);
  ledcSetup(greenChl, freq, res);
  ledcSetup(blueChl, freq, res);
  
  // matching LED pinout to GPIO
  ledcAttachPin(RED, redChl);
  ledcAttachPin(GREEN, greenChl);
  ledcAttachPin(BLUE, blueChl);
  //ESC to GPIO
  esc.setPeriodHertz(50);
  esc.attach(ESC_PIN, 1000, 2000);
  esc.writeMicroseconds(1000);
}

void setLED(const int* arr, bool blinkBool){ //if false, solid. if true, blink
  blink = blinkBool;
  red = arr[0];
  green = arr[1];
  blue = arr[2];
  if (blink == false){
    ledcWrite(redChl, red);
    ledcWrite(greenChl, green);  
    ledcWrite(blueChl, blue);
  }
  else if (blink == true){
    ledState = true;
    timeLastLED = millis(); //count
    ledcWrite(redChl, red); //initialize blink, turn on
    ledcWrite(greenChl, green);  
    ledcWrite(blueChl, blue);
  }
}

void updateCurrentState() {
  //state 0 = idle
  //state 1 = speed select
  //state 2 = time select
  //state 3 = active/running
  ok.update();
  cancel.update();
  plus.update();
  minus.update();

  if (state == 0) {
    //LED set to white
    setLED(whiteCode, false);
    //prepare/prompt for OK press
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("-----Spincoater-----");
    display.println("");
    display.println("STATUS: [Idle]");
    display.println("TIME: [0:00]");
    display.println("SPEED: [500RPM]");
    display.println("");
    display.print("[CANCEL] [-] [+] ");
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print("[OK]");
    display.display();
    if (ok.pressed()){
      state = 1;
      setLED(blueCode, true);
    }
  }
  else if (state == 1){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("-----Spincoater-----");
    display.println("");
    display.println("STATUS: [Timeset]");
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println("TIME: [0:00]");
    display.setTextColor(SSD1306_WHITE);
    display.println("SPEED: [500RPM]");
    display.println("");
    display.print("[CANCEL] [-] [+] [OK]");
    display.display();
    if (ok.pressed()){
      state = 2;
      setLED(yellowCode, true);
    }
  }
  else if (state == 2){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("-----Spincoater-----");
    display.println("");
    display.println("STATUS: [RPMset]");
    display.println("TIME: [0:00]");
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println("SPEED: [500RPM]");
    display.setTextColor(SSD1306_WHITE);
    display.println("");
    display.print("[CANCEL] [-] [+] [OK]");
    display.display();
    if (ok.pressed()){
      state = 3;
      setLED(greenCode, true);
    }
  }
  else if (state == 3){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("-----Spincoater-----");
    display.println("");
    display.println("STATUS: [RUN]");
    display.println("TIME: [x:xx]");
    display.println("SPEED: [xxxRPM]");
    display.println("");
    display.print("[CANCEL] [-] [+] ");
    display.print("[OK]");
    display.display();
  }
}

void blinkToggler(){ //code for blink based on millis()
  if (blink == false){
    return;
  }
  if ((millis() - timeLastLED) >= blinkDuration){
    timeLastLED = millis();
    ledState = !ledState;
    if (ledState){
      ledcWrite(redChl, red);  
      ledcWrite(greenChl, green);  
      ledcWrite(blueChl, blue);
    }
    else{
      ledcWrite(redChl, 0);
      ledcWrite(greenChl, 0);
      ledcWrite(blueChl, 0);
    }
  }
}

void loop() {
  updateCurrentState();
  blinkToggler();
}