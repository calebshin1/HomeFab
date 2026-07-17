#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64   
#define OLED_RESET    -1   // my ssd1306 has no reset pin
#define SCREEN_ADDRESS 0x3C 

#define OK 4
#define CANCEL 5
#define MINUS 18
#define PLUS 19

#define RED 25
#define GREEN 26
#define BLUE 27

const int freq = 5000;
const int res = 8;

const int redChl = 0;
const int greenChl = 1;
const int blueChl = 2;

uint8_t state = 0;

#define LED_PIN 2
bool ledState = LOW;

Bounce2::Button ok = Bounce2::Button();
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

  //sample OLED layout testing
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  // display.println("-----Spincoater-----");
  // display.println("");
  // display.println("STATUS: [Idle]");
  // display.println("TIME: [0:00]");
  // display.println("SPEED: [500RPM]");
  // display.println("");
  // display.println("[CANCEL] [-] [+] [OK]");
  display.println("Button Testing:");
  
  display.display();

  ledcSetup(redChl, freq, res);
  ledcSetup(greenChl, freq, res);
  ledcSetup(blueChl, freq, res);
  
  // Attach the channels to the GPIO pins
  ledcAttachPin(RED, redChl);
  ledcAttachPin(GREEN, greenChl);
  ledcAttachPin(BLUE, blueChl);
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
    }
  }
  else if (state == 1){
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("-----Spincoater-----");
    display.println("");
    display.println("STATUS: [Idle]");
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.println("TIME: [0:00]");
    display.setTextColor(SSD1306_WHITE);
    display.println("SPEED: [500RPM]");
    display.println("");
    display.print("[CANCEL] [-] [+] [OK]");
    display.display();
  }
}

void loop() {
  updateCurrentState();
  ledcWrite(redChl, 255);
  ledcWrite(greenChl, 255);
  ledcWrite(blueChl, 255);
}