// Exercise the production display setup, including both framebuffer paths.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "display.h"

DisplayTestProbe displayTestProbe;
TestSerial Serial;
bool displayTestAllowAllocation = true;
int displayTestAllocationAttempts = 0;
int backlightModeCalls = 0, backlightWriteCalls = 0;
void pinMode(int pin, int mode) {
  assert(pin == LCD_BL && mode == OUTPUT);
  backlightModeCalls++;
}
void digitalWrite(int pin, int value) {
  assert(pin == LCD_BL && value == HIGH);
  backlightWriteCalls++;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const bool fallback = strcmp(argv[1], "fallback") == 0;
  assert(fallback || strcmp(argv[1], "canvas") == 0);
  displayTestAllowAllocation = !fallback;
  displayBegin();

  assert(backlightModeCalls == 1 && backlightWriteCalls == 1);
  assert(displayTestProbe.dc == LCD_DC && displayTestProbe.cs == LCD_CS);
  assert(displayTestProbe.sck == LCD_SCK && displayTestProbe.mosi == LCD_MOSI);
  assert(displayTestProbe.miso == LCD_MISO && displayTestProbe.reset == GFX_NOT_DEFINED);
  assert(displayTestProbe.rotation == 0 && displayTestProbe.ips);
  assert(!INVERT_COLOURS && !displayTestProbe.requestedInversion);
  assert(displayTestProbe.command == 0x21); // ES3C28P IPS panel's normal polarity.
  // The explicit setting must reach the hardware even when gfx is a canvas.
  assert(displayTestProbe.panelInversions == displayTestProbe.panelBegins + 1);
  assert(displayTestProbe.canvasInversions == 0);
  assert(displayTestProbe.panelBegins == (fallback ? 2 : 1));
  assert(displayTestAllocationAttempts == (fallback ? 3 : 1));
  assert(displayUsingCanvas() == !fallback);
  assert((displayFramebuffer() != nullptr) == !fallback);
  assert(gfx == (fallback ? displayTestProbe.panel : displayTestProbe.canvas));
  displayFlush();
  assert(displayTestProbe.canvasFlushes == (fallback ? 0 : 1));
  printf("PASS: display IPS polarity and hardware inversion (%s)\n", argv[1]);
}
