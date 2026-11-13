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

// ---------- Colours (RGB565) ----------
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_GREY    0x8410
