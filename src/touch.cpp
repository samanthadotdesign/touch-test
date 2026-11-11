#include "touch.h"
#include "config.h"
#include "display.h"
#include <Wire.h>

void touchBegin() {
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(300);
  Wire.begin(TP_SDA, TP_SCL);

  Wire.beginTransmission(TP_ADDR);
  bool found = (Wire.endTransmission() == 0);
  Serial.println(found ? "Touch chip found at 0x38" : "Touch chip NOT found");
  if (!found) {
    Serial.println("Touch chip not at 0x38. Scanning I2C bus...");
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) Serial.printf("  found device at 0x%02X\n", a);
    }
    Serial.println("Scan done.");
  }
}

bool touchRead(int16_t &x, int16_t &y) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)5) != 5) return false;

  uint8_t points = Wire.read() & 0x0F;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  if (points == 0 || points > 2) return false;

  int16_t rx = ((xh & 0x0F) << 8) | xl;
  int16_t ry = ((yh & 0x0F) << 8) | yl;

  if (SWAP_XY) { int16_t t = rx; rx = ry; ry = t; }
  if (FLIP_X) rx = LCD_W - 1 - rx;
  if (FLIP_Y) ry = LCD_H - 1 - ry;

  x = constrain(rx, 0, LCD_W - 1);
  y = constrain(ry, 0, LCD_H - 1);
  return true;
}

void touchWaitRelease() {
  int16_t tx, ty;
  while (touchRead(tx, ty)) delay(20);
  delay(80);
}

bool touchInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
