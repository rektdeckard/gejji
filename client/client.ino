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
#define NET_UP 2
#define NET_DOWN 3
#define DISK_READ 4
#define DISK_WRITE 5

#define A_BUTTON 0
#define B_BUTTON 16
#define C_BUTTON 2

enum View
{
  SYSTEM,
  NETWORK,
  DISK,
};

int retries = 0;
bool alphanum = true;
int idx = 0;
int data[6][128];
View view = SYSTEM;

volatile bool displayImmediate = false;
volatile bool showInfo = true;
volatile unsigned long lastTrigger = millis();
volatile int bounce = 0;
int prevBounceCount = 0;

// clang-format off
static const unsigned char PROGMEM logo[] =
{
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11111111,0b11111111,0b00000000,0b00000000,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b00000000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11110000,0b11110000,0b11111111,0b00000000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b00001111,
  0b11110000,0b11110000,0b11111111,0b00000000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b00001111,
  0b11110000,0b11110000,0b11111111,0b00000000,0b11111111,0b00001111,0b11111111,0b11111111,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b00001111,
  0b11110000,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b00001111,0b11110000,0b11111111,0b00001111,0b11111111,0b00001111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b00001111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b00001111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b11111111,0b11110000,0b11111111,0b00001111,0b11111111,0b11110000,0b11111111,0b00001111,0b00001111,0b11110000,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11110000,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b00000000,0b11111111,0b11110000,0b00001111,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b00000000,0b11111111,0b11110000,0b00001111,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b00000000,0b11111111,0b11110000,0b00001111,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b00000000,0b00001111,0b11111111,0b00000000,0b00000000,0b11111111,0b00000000,0b11111111,0b11110000,0b00001111,0b11111111,0b00001111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
  0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,0b11111111,
};
// clang-format on

ICACHE_RAM_ATTR void changeView()
{
  if (lastTrigger > millis() || lastTrigger + 200 < millis())
  {
    lastTrigger = millis();
    if (view == SYSTEM)
    {
      view = NETWORK;
    }
    else if (view == NETWORK)
    {
      view = DISK;
    }
    else
    {
      view = SYSTEM;
    }

    if (!alphanum)
    {
      displayImmediate = true;
    }
  }
  else
  {
    bounce++;
  }
}

ICACHE_RAM_ATTR void toggleInfo()
{
  if (lastTrigger > millis() || lastTrigger + 200 < millis())
  {
    lastTrigger = millis();
    showInfo = !showInfo;

    if (!alphanum)
    {
      displayImmediate = true;
    }
  }
  else
  {
    bounce++;
  }
}

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

  display.clearDisplay();
  display.drawBitmap(0, 0, logo, 128, 32, 1);
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);
  display.cp437(true);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

  pinMode(A_BUTTON, INPUT_PULLUP);
  pinMode(C_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(A_BUTTON), toggleInfo, RISING);
  attachInterrupt(digitalPinToInterrupt(C_BUTTON), changeView, RISING);
}

void setup()
{
  memset(data, 0, sizeof(int) * 4 * 128);
  Serial.begin(115200);

  while (!Serial.available())
  {
    // Wait for first packet
    delay(1000);
  }

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

void paintOLED()
{
  display.clearDisplay();

  if (view == SYSTEM)
  {
    for (int i = idx, x = 0; x < display.width(); --i, ++x)
    {
      if (i < 0)
        i = display.width() - 1;

      int16_t pos = display.width() - x - 1;
      display.drawLine(pos, display.height(), pos, display.height() - (data[CPU][i] * display.height()) / 2000, SSD1306_WHITE);
      display.drawLine(pos, 0, pos, (data[MEM][i] * display.height()) / 2000, SSD1306_WHITE);
    }

    if (showInfo)
    {
      display.setCursor(2, 4);
      display.printf("MEM %d%%", data[MEM][idx] / 10);
      display.setCursor(2, 20);
      display.printf("CPU %d%%", data[CPU][idx] / 10);
    }
  }
  else if (view == NETWORK)
  {
    int max_up = 10;
    int max_down = 10;

    for (int n = 0; n < 128; n++)
    {
      if (data[NET_UP][n] > max_up)
      {
        max_up = data[NET_UP][n];
      }
      if (data[NET_DOWN][n] > max_down)
      {
        max_down = data[NET_DOWN][n];
      }
    }

    for (int i = idx, x = 0; x < display.width(); --i, ++x)
    {
      if (i < 0)
        i = display.width() - 1;

      int16_t pos = display.width() - x - 1;

      if (max_up > 0)
      {
        display.drawLine(pos, 0, pos, (data[NET_UP][i] * display.height()) / (2 * max_up), SSD1306_WHITE);
      }
      if (max_down > 0)
      {
        display.drawLine(pos, display.height(), pos, display.height() - (data[NET_DOWN][i] * display.height()) / (2 * max_down), SSD1306_WHITE);
      }
    }

    if (showInfo)
    {
      display.setCursor(2, 4);
      int net_up = data[NET_UP][idx];
      if (net_up > 1023)
      {
        display.write(0x1E);
        display.printf(" %.1f MB/s", net_up / 1024.0);
      }
      else
      {
        display.write(0x1E);
        display.printf(" %d KB/s", net_up);
      }
      display.setCursor(2, 20);
      int net_down = data[NET_DOWN][idx];
      if (net_down > 1024)
      {
        display.write(0x1F);
        display.printf(" %.1f MB/s", net_down / 1024.0);
      }
      else
      {
        display.write(0x1F);
        display.printf(" %d KB/s", data[NET_DOWN][idx]);
      }
    }
  }
  else
  {
    int max_up = 10;
    int max_down = 10;

    for (int n = 0; n < 128; n++)
    {
      if (data[DISK_READ][n] > max_up)
      {
        max_up = data[DISK_READ][n];
      }
      if (data[DISK_WRITE][n] > max_down)
      {
        max_down = data[DISK_WRITE][n];
      }
    }

    for (int i = idx, x = 0; x < display.width(); --i, ++x)
    {
      if (i < 0)
        i = display.width() - 1;

      int16_t pos = display.width() - x - 1;

      if (max_up > 0)
      {
        display.drawLine(pos, 0, pos, (data[DISK_READ][i] * display.height()) / (2 * max_up), SSD1306_WHITE);
      }
      if (max_down > 0)
      {
        display.drawLine(pos, display.height(), pos, display.height() - (data[DISK_WRITE][i] * display.height()) / (2 * max_down), SSD1306_WHITE);
      }
    }

    if (showInfo)
    {
      display.setCursor(2, 4);
      int disk_read = data[DISK_READ][idx];
      if (disk_read > 1023)
      {
        display.printf("R %.1f MB/s", disk_read / 1024.0);
      }
      else
      {
        display.printf("R %d KB/s", disk_read);
      }
      display.setCursor(2, 20);
      int disk_write = data[DISK_WRITE][idx];
      if (disk_write > 1024)
      {
        display.printf("W %.1f MB/s", disk_write / 1024.0);
      }
      else
      {
        display.printf("W %d KB/s", data[DISK_WRITE][idx]);
      }
    }
  }

  display.display();
}

void loop()
{
  if (displayImmediate && !alphanum)
  {
    displayImmediate = false;
    paintOLED();
  }

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

  JsonObject root = doc.as<JsonObject>();
  int cpu = root["cpu"].as<int>();
  int mem = root["mem"].as<int>();
  int interval = root["interval"].as<int>();
  int bri = root["bri"].as<int>();
  int net_up = root["net_up"].as<int>();
  int net_down = root["net_down"].as<int>();
  int disk_read = root["disk_read"].as<int>();
  int disk_write = root["disk_write"].as<int>();

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
    if (idx >= display.width())
      idx = 0;

    data[CPU][idx] = cpu;
    data[MEM][idx] = mem;
    data[NET_UP][idx] = net_up;
    data[NET_DOWN][idx] = net_down;
    data[DISK_READ][idx] = disk_read;
    data[DISK_WRITE][idx] = disk_write;

    paintOLED();
    idx += 1;
  }
}
