#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64   
#define OLED_RESET    -1   // my ssd1306 has no reset pin
#define SCREEN_ADDRESS 0x3C 

//RGB LED (5TB4URGB) GPIO pins used on ESP32
#define RED 25
#define GREEN 26
#define BLUE 27

//PTS645 GPIO pins used on ESP32
#define OK 4
#define CANCEL 5
#define MINUS 18
#define PLUS 19

//ESC signal pin
#define ESC_PIN 14 

//HE sens pin
#define HALL_PIN 33
const int magnetsPerRev = 2;   

//LED PWM params
const int freq = 5000;
const int res = 8;
const int redChl = 4;
const int greenChl = 5;
const int blueChl = 6;
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
//LED color arrays (for easier organization)
const int whiteCode[3] = {60,60,60};
const int blueCode[3] = {0,0,90};
const int yellowCode[3] = {90,60,0};
const int greenCode[3] = {0,60,0};
const int offCode[3] = {0,0,0};

bool blink {}; //boolean for if blink mode or not
bool ledState {}; //for if led is on or not
unsigned long timeLastLED = 0; //value that tracks time between LED toggles
const unsigned long blinkDuration = 500; //ms

Bounce2::Button ok = Bounce2::Button(); //init buttons via bounce2 library
Bounce2::Button cancel = Bounce2::Button();
Bounce2::Button two = Bounce2::Button();
Bounce2::Button one = Bounce2::Button();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); //init OLED
Servo esc; //init motor class

volatile unsigned long lastPulseTime = 0; //for motor commands
volatile unsigned long pulseInterval = 0;

void IRAM_ATTR onPulse() { //update pulse times
  unsigned long now = micros();
  pulseInterval = now - lastPulseTime;
  lastPulseTime = now;
}

float getRPM() { //read RPM real time 
  noInterrupts();
  unsigned long interval = pulseInterval;
  unsigned long last = lastPulseTime;
  interrupts();
  if (interval == 0 || micros() - last > 500000) return 0; 
  float pulsesPerSec = 1000000.0 / interval;
  return (pulsesPerSec / magnetsPerRev) * 60.0;
}

//spin params
enum AppState { IDLE, FLASH_SELECT, ARM_WAIT, ARM_SENDING, RUNNING }; //machine states for easy separation of processes
AppState appState = IDLE; //default idle

int targetRPM = 0;           //set later before run
int throttleUs = 1300;       //initial "low throttle" value
const int maxStepPerLoop = 15;
unsigned long armStartMillis = 0; //empty value for arming timer
const unsigned long ARM_DELAY_MS = 3000; //arm delay, 3s
unsigned long runStartMillis = 0; //empty value for event run timer
const unsigned long RUN_DURATION_MS = 60000; // 60s

//misc LED elements for timing blinks
unsigned long flashStartMillis = 0;
const unsigned long FLASH_DURATION_MS = 500;

// PID state
float integral = 0;
const float Kp = 0.1;
const float Ki = 0.03;
unsigned long lastPidMillis = 0;
const unsigned long PID_INTERVAL_MS = 50;

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
    timeLastLED = millis(); 
    ledcWrite(redChl, red); 
    ledcWrite(greenChl, green);  
    ledcWrite(blueChl, blue);
  }
}

int setRPM = 0;
void startRun(int rpm) {
  targetRPM = rpm;
  throttleUs = 1000;
  integral = 0;
  esc.writeMicroseconds(1000);
  armStartMillis = millis();
  appState = ARM_WAIT;
}

void stopRun() {
  setLED(offCode, false);
  esc.writeMicroseconds(1000);
  throttleUs = 1000;
  appState = IDLE;
  setLED(offCode, false);
}

void updatePID() {
  Serial.print("target: "); Serial.print(targetRPM);
  Serial.print("  rpm: "); Serial.print(getRPM());
  Serial.print("  throttle: "); Serial.println(throttleUs);
  if (millis() - lastPidMillis < PID_INTERVAL_MS) return;
  lastPidMillis = millis();

  float rpm = getRPM();
  float error = targetRPM - rpm;
  integral += error * (PID_INTERVAL_MS / 1000.0);
  if (throttleUs < 2000 && throttleUs > 1000) {
    integral += error * (PID_INTERVAL_MS / 1000.0);
  }
  integral = constrain(integral, -800, 800); 

  int desiredThrottle = 1000 + (int)((Kp * error) + (Ki * integral));
  desiredThrottle = constrain(desiredThrottle, 1000, 2000);

  throttleUs = constrain(desiredThrottle, throttleUs - maxStepPerLoop, throttleUs + maxStepPerLoop);
  throttleUs = constrain(throttleUs, 1000, 2000);
  esc.writeMicroseconds(throttleUs);

  Serial.print("target: "); Serial.print(targetRPM);
  Serial.print("  rpm: "); Serial.print(rpm);
  Serial.print("  throttle: "); Serial.println(throttleUs);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup..");

  ok.attach(OK, INPUT_PULLUP);
  ok.interval(5);
  ok.setPressedState(LOW);

  cancel.attach(CANCEL, INPUT_PULLUP);
  cancel.interval(5);
  cancel.setPressedState(LOW);

  two.attach(PLUS, INPUT_PULLUP);
  two.interval(5);
  two.setPressedState(LOW);

  one.attach(MINUS, INPUT_PULLUP);
  one.interval(5);
  one.setPressedState(LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true) delay(10);
  }

  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), onPulse, FALLING);

  ledcSetup(redChl, freq, res);
  ledcSetup(greenChl, freq, res);
  ledcSetup(blueChl, freq, res);
  
  // matching LED pinout to GPIO
  ledcAttachPin(RED, redChl);
  ledcAttachPin(GREEN, greenChl);
  ledcAttachPin(BLUE, blueChl);
  setLED(offCode, false);
  
  // ESC init
  esc.setPeriodHertz(50);
  esc.attach(ESC_PIN, 1000, 2000);
  esc.writeMicroseconds(1000);
  Serial.println("Setup complete.");
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

void drawStatus(unsigned long elapsedMs, float rpm) {
  unsigned long elapsedSec = elapsedMs / 1000;
  unsigned int mm = elapsedSec / 60;
  unsigned int ss = elapsedSec % 60;

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 8);
  if (mm < 10) display.print("0");
  display.print(mm); display.print(":");
  if (ss < 10) display.print("0");
  display.print(ss);

  display.setCursor(0, 36);
  display.print((int)rpm);
  display.print(" RPM");

  display.display();
}

void loop() {
  ok.update();
  cancel.update();
  two.update();
  one.update();
  blinkToggler();

  if (appState == IDLE) {
    drawStatus(0, 0);
    if (two.pressed()) {
      setRPM = 3000;
      setLED(whiteCode, false);     
      flashStartMillis = millis();
      appState = FLASH_SELECT;
    }
    if (one.pressed()) {
      setRPM = 4000;
      setLED(whiteCode, false);     
      flashStartMillis = millis();
      appState = FLASH_SELECT;
    }
  }
    else if (appState == FLASH_SELECT) {
    drawStatus(0, 0);
    if (millis() - flashStartMillis >= FLASH_DURATION_MS) {
      setLED(offCode, false);
      startRun(setRPM);  
    }
  }
  else if (appState == ARM_WAIT) {
    esc.writeMicroseconds(1000);
    armStartMillis = millis();
    appState = ARM_SENDING;
    setLED(yellowCode, true); 
    drawStatus(0, 0);
  }
  else if (appState == ARM_SENDING) {
    drawStatus(0, 0);
    if (cancel.pressed()) {
      stopRun();
    }
    else if (millis() - armStartMillis >= ARM_DELAY_MS) {
      runStartMillis = millis();
      lastPidMillis = millis();
      appState = RUNNING;
      setLED(greenCode, true);   
    }
  }
  else if (appState == RUNNING) {
    updatePID();

    unsigned long elapsed = millis() - runStartMillis;
    drawStatus(elapsed, getRPM());

    if (cancel.pressed()) {
      stopRun();
    }
    else if (elapsed >= RUN_DURATION_MS) {
      stopRun();
    }
  }
}
