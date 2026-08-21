#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MAX31855.h>

//MAX31855
#define MAXDO   19   // DO  
#define MAXCS   5    // CS  
#define MAXCLK  18   // CLK 

Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

//SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64   
#define OLED_RESET    -1   
#define SCREEN_ADDR   0x3C 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//I2C pins on ESP32
#define I2C_SCL 21
#define I2C_SDA 22

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    while (true) delay(10);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  delay(500);
  Serial.println("MAX31855 + SSD1306 thermometer starting");
}

void loop() {
  double tempC = thermocouple.readCelsius();
  double tempF = thermocouple.readFahrenheit();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Thermocouple Temp");

  if (isnan(tempC)) {
    uint8_t err = thermocouple.readError();
    display.setCursor(0, 20);
    display.println("Read error!");
    if (err & MAX31855_FAULT_OPEN)    display.println("- Open circuit");
    if (err & MAX31855_FAULT_SHORT_GND) display.println("- Short to GND");
    if (err & MAX31855_FAULT_SHORT_VCC) display.println("- Short to VCC");

    Serial.println("Thermocouple read error");
  } else {
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(tempC, 1);
    display.println(" C");

    display.setCursor(0, 44);
    display.print(tempF, 1);
    display.println(" F");

    Serial.print("Temp: ");
    Serial.print(tempC);
    Serial.print(" C / ");
    Serial.print(tempF);
    Serial.println(" F");
  }

  display.display();

  delay(500); 
}
