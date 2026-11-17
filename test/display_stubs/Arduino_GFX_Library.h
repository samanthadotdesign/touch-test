#pragma once
#include "Arduino.h"

#define GFX_NOT_DEFINED -1
#define GFX_SKIP_OUTPUT_BEGIN -2

struct DisplayTestProbe {
  int dc = -1, cs = -1, sck = -1, mosi = -1, miso = -1;
  int reset = 0, rotation = -1;
  bool ips = false, requestedInversion = true;
  int panelBegins = 0, panelInversions = 0, canvasInversions = 0;
  int canvasFlushes = 0;
  uint8_t command = 0;
  void *panel = nullptr, *canvas = nullptr;
};
extern DisplayTestProbe displayTestProbe;

class Arduino_DataBus {
 public:
  virtual ~Arduino_DataBus() {}
};
class Arduino_ESP32SPI : public Arduino_DataBus {
 public:
  Arduino_ESP32SPI(int dc, int cs, int sck, int mosi, int miso) {
    displayTestProbe.dc = dc; displayTestProbe.cs = cs;
    displayTestProbe.sck = sck; displayTestProbe.mosi = mosi; displayTestProbe.miso = miso;
  }
};
class Arduino_GFX {
 public:
  virtual ~Arduino_GFX() {}
  virtual bool begin(int32_t speed = GFX_NOT_DEFINED) = 0;
  virtual void invertDisplay(bool) {}
  virtual void flush() {}
  void setTextSize(uint8_t) {}
  void setTextColor(uint16_t, uint16_t) {}
  void setCursor(int16_t, int16_t) {}
  size_t print(const char *text) { return strlen(text); }
};
class Arduino_ILI9341 : public Arduino_GFX {
 public:
  Arduino_ILI9341(Arduino_DataBus *, int8_t reset = GFX_NOT_DEFINED,
                 uint8_t rotation = 0, bool ips = false) : ipsPanel(ips) {
    displayTestProbe.panel = this;
    displayTestProbe.reset = reset;
    displayTestProbe.rotation = rotation;
    displayTestProbe.ips = ips;
  }
  bool begin(int32_t = GFX_NOT_DEFINED) override {
    displayTestProbe.panelBegins++;
    invertDisplay(false); // The real ILI9341 tftInit() applies this default.
    return true;
  }
  void invertDisplay(bool value) override {
    displayTestProbe.panelInversions++;
    displayTestProbe.requestedInversion = value;
    // Same panel command mapping as GFX Library for Arduino 1.4.9.
    displayTestProbe.command = (ipsPanel ^ value) ? 0x21 : 0x20;
  }
 private:
  bool ipsPanel;
};
class Arduino_Canvas : public Arduino_GFX {
 public:
  Arduino_Canvas(int16_t width, int16_t height, Arduino_GFX *output)
      : _width(width), _height(height), _output(output) { displayTestProbe.canvas = this; }
  bool begin(int32_t = GFX_NOT_DEFINED) override { return false; }
  void invertDisplay(bool) override { displayTestProbe.canvasInversions++; }
  void flush() override { displayTestProbe.canvasFlushes++; }
  uint16_t *getFramebuffer() { return _framebuffer; }
 protected:
  int16_t _width, _height;
  Arduino_GFX *_output;
  uint16_t *_framebuffer = nullptr;
};
