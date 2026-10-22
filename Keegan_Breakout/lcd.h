// lcd.h - simple driver for the EDUMKII 128x128 LCD
#ifndef LCD_H
#define LCD_H

#include <stdint.h>

// colors (RGB565)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define YELLOW  0xFFE0
#define GREEN   0x07E0
#define CYAN    0x07FF
#define BLUE    0x001F

void lcd_init(void);
void lcd_rect(int x, int y, int w, int h, uint16_t color);   // filled rectangle
void lcd_text(int x, int y, const char *s, uint16_t color);   // white-on-black text

#endif
