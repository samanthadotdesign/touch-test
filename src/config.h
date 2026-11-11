#pragma once

// 2.8" ESP32-S3 board (LCDwiki ES3C28P): ILI9341V 240x320 + FT6336G + ES8311

// ---------- Screen ----------
#define LCD_CS    10
#define LCD_DC    46
#define LCD_SCK   12
#define LCD_MOSI  11
#define LCD_MISO  13
#define LCD_BL    45
#define LCD_W     240
#define LCD_H     320

#define INVERT_COLOURS false

// ---------- Touch ----------
#define TP_SDA    16
#define TP_SCL    15
#define TP_RST    18
#define TP_ADDR   0x38
#define SWAP_XY   false
#define FLIP_X    false
#define FLIP_Y    false

#define BOOT_BTN  0

// ---------- Audio (ES8311 + SC8002B) — unused until Step 3 ----------
#define I2S_MCLK       4
#define I2S_BCLK       5
#define I2S_LRCK       7
#define I2S_DOUT       8
#define I2S_DIN        6
#define AMP_EN         1
#define ES8311_ADDR    0x18

// ---------- Timing ----------
// 1 = real time. 60 = one minute passes in one second (device testing).
#define DEV_TIME_SCALE 1

#define BOOT_MS           2000
#define SUMMARY_MS        5000
#define SIGN_IDLE_MS      3000
#define CLOCK_HOLD_MS     1000
#define SCATTER_MS        1100

#define FOCUS_MS          (25UL * 60UL * 1000UL)
#define BREAK_MS          (5UL  * 60UL * 1000UL)
#define WARN_AT_MS        (22UL * 60UL * 1000UL)

// Blow detection placeholders (mic stubbed; quote advances on tap)
#define BLOW_FACTOR       3.0f
#define BLOW_HOLD_MS      150

// ---------- Pause hit target ----------
#define PAUSE_HIT  48
#define PAUSE_X    ((LCD_W - PAUSE_HIT) / 2)
#define PAUSE_Y    (LCD_H - PAUSE_HIT - 6)

// ---------- Colours (RGB565) ----------
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_BLUE    0x001F
#define C_CYAN    0x07FF
#define C_GREY    0x8410
#define C_DARK    0x4208
#define C_YELLOW  0xFFE0

#define MAX_LETTERS    96
#define MAX_STROKES    1200
