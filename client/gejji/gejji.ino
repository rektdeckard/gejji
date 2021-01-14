#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();

void setup() {
  Serial.begin(9600);
  
  alpha4.begin(0x70);  // pass in the address
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

void loop() {
  if (!Serial.available()) {
    return;
  }

  String cpu_string = Serial.readStringUntil('\n');
  String mem_string = Serial.readStringUntil('\n');

  // Display CPU Usage
  alpha4.writeDigitAscii(2, '0', true);
  for (int i = cpu_string.length() - 1, j = 3; i >= 0 && j >= 0; i--, j--) {
      alpha4.writeDigitAscii(j, cpu_string[i], j == 2);
  }
  alpha4.writeDigitAscii(0, 'C');
  alpha4.writeDisplay();
  delay(5000);
  alpha4.clear();

  // Display Memory Usage
  alpha4.writeDigitAscii(2, '0', true);
  for (int i = mem_string.length() - 1, j = 3; i >= 0 && j >= 0; i--, j--) {
      alpha4.writeDigitAscii(j, mem_string[i], j == 2);
  }
  alpha4.writeDigitAscii(0, 'M');
  alpha4.writeDisplay();
  delay(5000);
  alpha4.clear();
}
