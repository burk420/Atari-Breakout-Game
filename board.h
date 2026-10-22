/*
 * board.h - Board-specific pins for MSP-EXP430FR5994 + BOOSTXL-EDUMKII.
 *
 * Header-to-MCU mapping is from TI's MSP-EXP430FR5994 user guide (SLAU678),
 * Figure 2-10, and cross-checked against the port names printed on the board.
 * BoosterPack MK II functions are from TI's BOOSTXL-EDUMKII guide (SLAU599).
 *
 *   Signal          Header   FR5994 pin
 *   LCD SPI CLK     J1.7     P5.2  (UCB1CLK)
 *   LCD SPI MOSI    J2.15    P5.0  (UCB1SIMO)
 *   LCD CS          J2.13    P8.3
 *   LCD RS (D/C)    J4.31    P4.1   (confirmed on hardware by the pin scan)
 *   LCD RST         J2.17    P5.3
 *   Backlight       J4.39    P3.6
 *   Joystick X      J1.2     P1.2  (A2)
 *   Button S2       J4.32    P4.2
 *
 */
#ifndef BOARD_H_
#define BOARD_H_

#include <msp430fr5994.h>

/* ---- LCD SPI: eUSCI_B1, CLK = P5.2, SIMO = P5.0 (primary function: SEL0 = 1) ---- */
#define LCD_SPI_CTLW0   UCB1CTLW0
#define LCD_SPI_BRW     UCB1BRW
#define LCD_SPI_STATW   UCB1STATW
#define LCD_SPI_TXBUF   UCB1TXBUF
#define LCD_SPI_RXBUF   UCB1RXBUF
#define LCD_SPI_IFG     UCB1IFG
#define LCD_SPI_SEL0    P5SEL0
#define LCD_SPI_SEL1    P5SEL1
#define LCD_SPI_PINS    (BIT0 | BIT2)

/* ---- LCD GPIO lines ---- */
/* CS: J2.13 = P8.3 */
#define LCD_CS_DIR  P8DIR
#define LCD_CS_OUT  P8OUT
#define LCD_CS_BIT  BIT3
/* RST: J2.17 = P5.3 (the pin scan confirmed D/C is J4.31, so reset is the other candidate) */
#define LCD_RST_DIR P5DIR
#define LCD_RST_OUT P5OUT
#define LCD_RST_BIT BIT3
/* RS / data-command: J4.31 = P4.1 (confirmed by the pin scan) */
#define LCD_DC_DIR  P4DIR
#define LCD_DC_OUT  P4OUT
#define LCD_DC_BIT  BIT1
/* Backlight: J4.39 = P3.6 (shared with RGB red via jumper J5) */
#define LCD_BL_DIR  P3DIR
#define LCD_BL_OUT  P3OUT
#define LCD_BL_BIT  BIT6

/* ---- Button S2: J4.32 = P4.2, active low, interrupt on press ---- */
#define BTN_IN      P4IN
#define BTN_DIR     P4DIR
#define BTN_REN     P4REN
#define BTN_OUT     P4OUT
#define BTN_IES     P4IES
#define BTN_IE      P4IE
#define BTN_IFG     P4IFG
#define BTN_BIT     BIT2
#define BTN_VECTOR  PORT4_VECTOR

/* ---- Joystick X: J1.2 = P1.2 = A2 ---- */
#define JOY_SEL0    P1SEL0
#define JOY_SEL1    P1SEL1
#define JOY_BIT     BIT2
#define JOY_INCH    ADC12INCH_2

/* ---- Buzzer: J4.40 = P3.7 = TB0.6 (Timer B0 PWM). Comment out BUZZER_ENABLED to mute. ---- */
#define BUZZER_ENABLED
//#define BUZZER_GPIO_TEST      /* 1 s, 4 kHz plain-GPIO buzz at power-up: tests the pin/buzzer without the timer. Comment out when done. */
//#define BUZZER_STARTUP_TEST   /* plays the win jingle at power-up; comment out once it works */
#define BUZZER_DIR      P3DIR
#define BUZZER_OUT      P3OUT
#define BUZZER_SEL0     P3SEL0
#define BUZZER_SEL1     P3SEL1
#define BUZZER_BIT      BIT7
#define BUZZER_TB_CTL   TB0CTL
#define BUZZER_TB_CCR0  TB0CCR0
#define BUZZER_TB_CCR   TB0CCR6
#define BUZZER_TB_CCTL  TB0CCTL6

/* ---- Debug LED: red LED (P1.0) on the LaunchPad ---- */
#define LED_DIR     P1DIR
#define LED_OUT     P1OUT
#define LED_BIT     BIT0

/* SPI clock divider: SPI clock = SMCLK / (LCD_SPI_DIV + 1). 0 = 8 MHz. Try 3 (2 MHz) if wiring is long/flaky. */
#define LCD_SPI_DIV 0

/* ---- Busy-wait delay (MCLK = 8 MHz, set in main.c) ---- */
#define MCLK_HZ 8000000UL
#define DELAY_MS(ms) do { unsigned int _d = (ms); while (_d--) __delay_cycles(MCLK_HZ / 1000); } while (0)

/* Uncomment to test only the LCD: after init the screen cycles red/green/blue/white
 * and the game never starts. */
//#define LCD_SELFTEST

/* Pin scan: uncomment to find the real LCD data/command pin. See PORTING.md. Implies LCD_SELFTEST. */
/* #define LCD_PINSCAN */

#ifdef LCD_PINSCAN
#include <stdint.h>
extern volatile uint8_t *lcd_dc_dir_p, *lcd_dc_out_p;
extern uint8_t lcd_dc_bit_v;
#undef  LCD_DC_DIR
#undef  LCD_DC_OUT
#undef  LCD_DC_BIT
#define LCD_DC_DIR  (*lcd_dc_dir_p)
#define LCD_DC_OUT  (*lcd_dc_out_p)
#define LCD_DC_BIT  lcd_dc_bit_v
#endif

#endif /* BOARD_H_ */
