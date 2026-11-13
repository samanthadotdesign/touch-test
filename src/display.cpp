#include "display.h"
#include <esp_heap_caps.h>

Arduino_GFX *gfx = nullptr;

static Arduino_DataBus *bus = nullptr;
static Arduino_ILI9341 *tft = nullptr;
static Arduino_Canvas *canvas = nullptr;
static bool useCanvas = false;

class PsamCanvas : public Arduino_Canvas {
 public:
  using Arduino_Canvas::Arduino_Canvas;
  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    if ((speed != GFX_SKIP_OUTPUT_BEGIN) && _output) {
      if (!_output->begin(speed)) return false;
    }
    if (!_framebuffer) {
      size_t s = (size_t)_width * (size_t)_height * 2;
      _framebuffer = (uint16_t *)heap_caps_aligned_alloc(
          16, s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!_framebuffer) {
        _framebuffer = (uint16_t *)heap_caps_malloc(
            s, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      }
      if (!_framebuffer) {
        _framebuffer = (uint16_t *)heap_caps_aligned_alloc(
            16, s, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      }
    }
    return _framebuffer != nullptr;
  }
};

void displayBegin() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO);
  tft = new Arduino_ILI9341(bus, GFX_NOT_DEFINED, 0);
  canvas = new PsamCanvas(LCD_W, LCD_H, tft);

  if (canvas->begin() && canvas->getFramebuffer()) {
    gfx = canvas;
    useCanvas = true;
    Serial.println("display: PSRAM canvas 240x320");
  } else {
    if (!tft->begin()) Serial.println("Screen init failed");
    gfx = tft;
    useCanvas = false;
    Serial.println("display: direct ILI9341 (no canvas)");
  }
  gfx->invertDisplay(INVERT_COLOURS);
}

void displayFlush() {
  if (useCanvas) canvas->flush();
}

bool displayUsingCanvas() { return useCanvas; }

void displayCenter(const char *s, int16_t y, uint8_t size, uint16_t fg, uint16_t bg) {
  int16_t w = (int16_t)strlen(s) * 6 * size;
  gfx->setTextSize(size);
  gfx->setTextColor(fg, bg);
  gfx->setCursor((LCD_W - w) / 2, y);
  gfx->print(s);
}
