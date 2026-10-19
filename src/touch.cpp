#include "touch.h"

#include "log.h"

#include <Arduino.h>
#include <Wire.h>

#include "lcd.h"

namespace touch {
namespace {

constexpr int PIN_SDA = 19;
constexpr int PIN_SCL = 20;
constexpr int PIN_RST = 38;

constexpr uint16_t REG_X_MAX = 0x8048;
constexpr uint16_t REG_STATUS = 0x814E;
constexpr uint16_t REG_POINT1 = 0x8150;

uint8_t address = 0;
// Auflösung, auf die der GT911 eingestellt ist (bei diesem Board 480x272 statt 800x480)
int rawWidth = lcd::WIDTH;
int rawHeight = lcd::HEIGHT;

bool readRegs(uint16_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

void writeReg(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(value);
  Wire.endTransmission();
}

}  // namespace

bool begin() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(60);

  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  // Die I2C-Adresse hängt vom Pegel am INT-Pin beim Reset ab
  for (uint8_t candidate : {0x5D, 0x14}) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      address = candidate;
      break;
    }
  }
  if (!address) return false;

  uint8_t res[4];
  if (readRegs(REG_X_MAX, res, sizeof(res))) {
    int w = res[0] | res[1] << 8, h = res[2] | res[3] << 8;
    if (w > 0 && h > 0) {
      rawWidth = w;
      rawHeight = h;
    }
  }
  logf("Touch GT911 an 0x%02X, Auflösung %dx%d", address, rawWidth, rawHeight);
  return true;
}

namespace {

enum class Raw { NoData, Released, Touched };

Raw readRaw(int& x, int& y) {
  if (!address) return Raw::NoData;
  uint8_t status;
  if (!readRegs(REG_STATUS, &status, 1) || !(status & 0x80)) return Raw::NoData;

  Raw result = Raw::Released;
  uint8_t p[4];
  if ((status & 0x0F) > 0 && readRegs(REG_POINT1, p, sizeof(p))) {
    x = (p[0] | p[1] << 8) * lcd::WIDTH / rawWidth;
    y = (p[2] | p[3] << 8) * lcd::HEIGHT / rawHeight;
    result = Raw::Touched;
  }
  writeReg(REG_STATUS, 0);
  return result;
}

constexpr int SWIPE_MIN = 90;      // Pixel waagerecht
constexpr int TAP_MAX = 30;        // Pixel Bewegung
constexpr uint32_t LOST_MS = 300;  // ohne Meldung gilt der Finger als losgelassen
constexpr uint32_t LONG_MS = 2000;

bool down = false;
bool longReported = false;
int startX, startY, lastX, lastY;
uint32_t lastSeen, downAt;

}  // namespace

Gesture poll(int& x, int& y) {
  int rx, ry;
  Raw raw = readRaw(rx, ry);
  if (raw == Raw::Touched) {
    if (!down) {
      down = true;
      longReported = false;
      startX = rx;
      startY = ry;
      downAt = millis();
    }
    lastX = rx;
    lastY = ry;
    lastSeen = millis();
    if (!longReported && millis() - downAt >= LONG_MS && abs(rx - startX) <= TAP_MAX && abs(ry - startY) <= TAP_MAX) {
      longReported = true;
      x = startX;
      y = startY;
      return Gesture::LongPress;
    }
    return Gesture::None;
  }
  if (!down || (raw == Raw::NoData && millis() - lastSeen < LOST_MS)) return Gesture::None;

  down = false;
  if (longReported) return Gesture::None;
  x = startX;
  y = startY;
  int dx = lastX - startX, dy = lastY - startY;
  if (abs(dx) >= SWIPE_MIN && abs(dx) > 2 * abs(dy)) return dx < 0 ? Gesture::SwipeLeft : Gesture::SwipeRight;
  if (abs(dx) <= TAP_MAX && abs(dy) <= TAP_MAX) return Gesture::Tap;
  return Gesture::None;
}

}  // namespace touch
