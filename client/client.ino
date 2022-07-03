#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include "Adafruit_LEDBackpack.h"
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display;

Adafruit_AlphaNum4 alpha4;
DynamicJsonDocument doc(256);

#define CPU 0
#define MEM 1

int retries = 0;
bool alphanum = true;
uint8_t idx = 0;
uint16_t data[2][128];

void initAlphanumericDisplay()
{
  Serial.println("Init alphanum");
  alpha4 = Adafruit_AlphaNum4();

  alpha4.begin(0x70); // pass in the address
  alpha4.setBrightness(15);

  alpha4.writeDigitRaw(3, 0x0);
  alpha4.writeDigitRaw(0, 0xFFFF);
  alpha4.writeDisplay();
  delay(200);
  alpha4.writeDigitRaw(0, 0x0);
  alpha4.writeDigitRaw(1, 0xFFFF);
  alpha4.writeDisplay();
  delay(200);
  alpha4.writeDigitRaw(1, 0x0);
  alpha4.writeDigitRaw(2, 0xFFFF);
  alpha4.writeDisplay();
  delay(200);
  alpha4.writeDigitRaw(2, 0x0);
  alpha4.writeDigitRaw(3, 0xFFFF);
  alpha4.writeDisplay();
  delay(200);

  alpha4.clear();
  alpha4.setBrightness(0);
  alpha4.writeDisplay();
}

void initOLEDDisplay()
{
  Serial.println("Init OLED");
  display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

void setup()
{
  Serial.begin(9600);

  while (!Serial.available())
  {
    // Wait for first packet
    delay(1000);
  }

  Serial.println("Got packets");

  auto error = deserializeJson(doc, Serial);
  if (error)
  {
    Serial.println(error.c_str());
    delay(50);
    return;
  }

  JsonObject root = doc.as<JsonObject>();
  alphanum = root["alphanum"].as<bool>();

  if (alphanum)
  {
    initAlphanumericDisplay();
  }
  else
  {
    initOLEDDisplay();
  }
}

void loop()
{
  if (!Serial.available())
  {
    retries += 1;

    if (retries > 20)
    {
      if (alphanum)
      {
        alpha4.clear();
        alpha4.writeDisplay();
        delay(950);
      }
    }

    delay(50);
    return;
  }

  retries = 0;

  auto error = deserializeJson(doc, Serial);
  if (error)
  {
    Serial.println(error.c_str());
    delay(50);
    return;
  }

  Serial.println("Got data!");

  JsonObject root = doc.as<JsonObject>();
  int cpu = root["cpu"].as<int>();
  int mem = root["mem"].as<int>();
  int interval = root["interval"].as<int>();
  int bri = root["bri"].as<int>();

  String cpu_string = String(cpu);
  String mem_string = String(mem);

  if (alphanum)
  {

    if (bri)
    {
      alpha4.setBrightness(bri);
    }

    // Display CPU Usage
    alpha4.writeDigitAscii(2, '0', true);
    for (int i = cpu_string.length() - 1, j = 3; i >= 0 && j >= 0; i--, j--)
    {
      alpha4.writeDigitAscii(j, cpu_string[i], j == 2);
    }
    alpha4.writeDigitAscii(0, 'C');
    alpha4.writeDisplay();
    delay(interval * 500);
    alpha4.clear();

    // Display Memory Usage
    alpha4.writeDigitAscii(2, '0', true);
    for (int i = mem_string.length() - 1, j = 3; i >= 0 && j >= 0; i--, j--)
    {
      alpha4.writeDigitAscii(j, mem_string[i], j == 2);
    }
    alpha4.writeDigitAscii(0, 'M');
    alpha4.writeDisplay();
    delay(interval * 500);
    alpha4.clear();
  }
  else
  {
    display.clearDisplay();
    if (idx >= display.width())
      idx = 0;

    data[CPU][idx] = cpu;
    data[MEM][idx] = mem;

    for (int i = idx, x = 0; x < display.width(); --i, ++x)
    {
      if (i < 0)
        i = display.width() - 1;

      int16_t pos = display.width() - x;
      display.drawLine(pos, display.height(), pos, display.height() - (data[CPU][i] * display.height()) / 2000, SSD1306_WHITE);
      display.drawLine(pos, 0, pos, (data[MEM][i] * display.height()) / 2000, SSD1306_WHITE);
    }

    display.setCursor(4, 4);
    display.printf("MEM %0g", mem / 10.0);
    display.setCursor(4, 20);
    display.printf("CPU %0g", cpu / 10.0);
    display.display();

    idx += 1;
  }
}
