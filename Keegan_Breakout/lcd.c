// lcd.c - simple SPI driver for the EDUMKII LCD (ST7735 controller)
//
// Pins:  SPI clock P5.2, SPI data P5.0 (eUSCI_B1)
//        CS P8.3, RESET P5.3, DC (command/data) P4.1, backlight P3.6

#include <msp430.h>
#include "lcd.h"

static void spi(uint8_t b)                 // send one byte
{
    while (!(UCB1IFG & UCTXIFG)) ;
    UCB1TXBUF = b;
}

static void done(void)                     // wait for the last byte to finish
{
    while (UCB1STATW & UCBUSY) ;
}

static void cmd(uint8_t c)                 // send a command byte
{
    done();
    P4OUT &= ~BIT1;                        // DC = 0 -> command
    P8OUT &= ~BIT3;                        // CS low
    spi(c);
    done();
    P8OUT |= BIT3;                         // CS high
    P4OUT |= BIT1;                         // DC = 1 -> data
}

static void data(uint8_t d)                // send a data byte
{
    P8OUT &= ~BIT3;
    spi(d);
    done();
    P8OUT |= BIT3;
}

void lcd_init(void)
{
    P8DIR |= BIT3;  P8OUT |= BIT3;         // CS
    P4DIR |= BIT1;  P4OUT |= BIT1;         // DC
    P5DIR |= BIT3;  P5OUT |= BIT3;         // RESET
    P3DIR |= BIT6;  P3OUT |= BIT6;         // backlight on

    P5SEL0 |= BIT0 | BIT2;                 // P5.0 = SPI data, P5.2 = SPI clock
    P5SEL1 &= ~(BIT0 | BIT2);
    UCB1CTLW0 = UCSWRST;
    UCB1CTLW0 |= UCCKPH | UCMSB | UCMST | UCSYNC | UCSSEL__SMCLK;
    UCB1BRW = 2;                           // 8 MHz / 2 = 4 MHz SPI
    UCB1CTLW0 &= ~UCSWRST;

    P5OUT &= ~BIT3; __delay_cycles(160000);   // reset the screen
    P5OUT |=  BIT3; __delay_cycles(1200000);

    cmd(0x01); __delay_cycles(1200000);    // software reset
    cmd(0x11); __delay_cycles(2040000);    // wake up
    cmd(0x3A); data(0x05);                 // 16-bit color
    cmd(0x36); data(0xC8);                 // screen orientation
    cmd(0x13);                             // normal display mode
    cmd(0x29); __delay_cycles(400000);     // display on
}

// fill a rectangle with one color (anything off the screen is cut off)
void lcd_rect(int x, int y, int w, int h, uint16_t color)
{
    long n;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 128) w = 128 - x;
    if (y + h > 128) h = 128 - y;
    if (w <= 0 || h <= 0) return;

    cmd(0x2A); data(0); data(x + 2); data(0); data(x + w + 1);   // columns
    cmd(0x2B); data(0); data(y + 3); data(0); data(y + h + 2);   // rows
    cmd(0x2C);                                                   // write pixels

    P8OUT &= ~BIT3;
    for (n = (long)w * h; n > 0; n--) {
        spi(color >> 8);
        spi(color);
    }
    done();
    P8OUT |= BIT3;
}

// 5x7 font, only the characters the game uses
static const struct { char c; uint8_t col[5]; } font[] = {
    {'0',{0x3E,0x51,0x49,0x45,0x3E}}, {'1',{0x00,0x42,0x7F,0x40,0x00}},
    {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}}, {'E',{0x7F,0x49,0x49,0x49,0x41}},
    {'I',{0x00,0x41,0x7F,0x41,0x00}}, {'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}}, {'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}}, {'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {':',{0x00,0x36,0x36,0x00,0x00}},
};

void lcd_text(int x, int y, const char *s, uint16_t color)
{
    for (; *s; s++, x += 6) {
        const uint8_t *g = 0;
        unsigned int i, row, col;
        for (i = 0; i < sizeof(font) / sizeof(font[0]); i++)
            if (font[i].c == *s) g = font[i].col;

        // draw the character one pixel at a time (6 wide x 8 tall)
        for (col = 0; col < 6; col++)
            for (row = 0; row < 8; row++) {
                int on = g && col < 5 && (g[col] & (1 << row));
                lcd_rect(x + col, y + row, 1, 1, on ? color : BLACK);
            }
    }
}
