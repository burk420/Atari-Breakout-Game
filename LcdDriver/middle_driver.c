// This code was ported from TI's sample code. See Copyright notice at the bottom of this file.

#include "board.h"
#include "LcdDriver/middle_driver.h"
#include "Grlib/grlib/grlib.h"
#include <stdint.h>

#ifdef LCD_PINSCAN
/* Candidate GPIO positions on the BoosterPack for the LCD data/command line.
 * During a scan, every candidate is pulsed together as "reset", and CS is held
 * low on both plausible CS pins. Only the selected candidate acts as D/C. */
typedef struct { volatile uint8_t *dir; volatile uint8_t *out; uint8_t bit; } ScanPin;
static const ScanPin scanPins[] = {
    { &P4DIR, &P4OUT, BIT1 },   /* 0: J4.31 P4.1 */
    { &P5DIR, &P5OUT, BIT3 },   /* 1: J2.17 P5.3 */
    { &P8DIR, &P8OUT, BIT1 },   /* 2: J2.11 P8.1 */
    { &P4DIR, &P4OUT, BIT4 },   /* 3: J2.18 P4.4 */
    { &P6DIR, &P6OUT, BIT2 },   /* 4: J1.5  P6.2 */
    { &P6DIR, &P6OUT, BIT3 },   /* 5: J1.8  P6.3 */
    { &P4DIR, &P4OUT, BIT2 },   /* 6: J4.32 P4.2 */
    { &P4DIR, &P4OUT, BIT3 },   /* 7: J4.33 P4.3 */
    { &P2DIR, &P2OUT, BIT5 }    /* 8: J4.34 P2.5 */
};
const unsigned char LCD_ScanCount = sizeof(scanPins) / sizeof(scanPins[0]);
volatile uint8_t *lcd_dc_dir_p = &P4DIR, *lcd_dc_out_p = &P4OUT;
uint8_t lcd_dc_bit_v = BIT1;

void LCD_PinScanSelect(unsigned char idx)
{
    lcd_dc_dir_p = scanPins[idx].dir;
    lcd_dc_out_p = scanPins[idx].out;
    lcd_dc_bit_v = scanPins[idx].bit;
}
#endif

void HAL_LCD_PortInit(void)
{
    /////////////////////////////////////
    // Configuring the SPI pins (eUSCI_B1: CLK + SIMO)
    /////////////////////////////////////
    // Primary module function: SEL1 = 0, SEL0 = 1
    LCD_SPI_SEL1 &= ~LCD_SPI_PINS;
    LCD_SPI_SEL0 |=  LCD_SPI_PINS;
    // SOMI/STE are not used: the display never sends data back and CS is a GPIO.

    ///////////////////////////////////////////////
    // Configuring the display's other pins
    ///////////////////////////////////////////////
    LCD_RST_DIR |= LCD_RST_BIT;  // reset
    LCD_DC_DIR  |= LCD_DC_BIT;   // data/command
    LCD_CS_DIR  |= LCD_CS_BIT;   // chip select
#ifdef LCD_PINSCAN
    {
        unsigned char i;
        for (i = 0; i < LCD_ScanCount; i++) *scanPins[i].dir |= scanPins[i].bit;
        P8DIR |= BIT2;           // second plausible CS (J2.12)
        P8OUT &= ~BIT2;
    }
#endif

    return;
}

// Hardware reset: low >= 10us (we use 10ms), then 120ms before any command.
void HAL_LCD_Reset(void)
{
#ifdef LCD_PINSCAN
    unsigned char i;
    for (i = 0; i < LCD_ScanCount; i++) *scanPins[i].out |=  scanPins[i].bit;
    DELAY_MS(5);
    for (i = 0; i < LCD_ScanCount; i++) *scanPins[i].out &= ~scanPins[i].bit;
    DELAY_MS(10);
    for (i = 0; i < LCD_ScanCount; i++) *scanPins[i].out |=  scanPins[i].bit;
    DELAY_MS(120);
#else
    LCD_RST_OUT |=  LCD_RST_BIT;
    DELAY_MS(5);
    LCD_RST_OUT &= ~LCD_RST_BIT;
    DELAY_MS(10);
    LCD_RST_OUT |=  LCD_RST_BIT;
    DELAY_MS(120);
#endif
}

void HAL_LCD_SpiInit(void)
{
    //////////////////////////
    // SPI configuration
    //////////////////////////

    // Put eUSCI in reset state while modifying the configuration
    LCD_SPI_CTLW0 |= UCSWRST;

    // Set clock phase to "capture on 1st edge, change on following edge"
    LCD_SPI_CTLW0 |=  BITF;
    // Set clock polarity to "inactive low"
    LCD_SPI_CTLW0 &= ~BITE;
    // Set data order to "transmit MSB first"
    LCD_SPI_CTLW0 |=  BITD;
    // Set MCU to "SPI master"
    LCD_SPI_CTLW0 |=  BITB;
    // Set SPI to "3 pin SPI" (we won't use eUSCI's chip select)
    LCD_SPI_CTLW0 &= ~(BIT9 | BITA);
    // Set module to synchronous mode
    LCD_SPI_CTLW0 |=  BIT8;
    // Set clock to SMCLK
    LCD_SPI_CTLW0 |=  BIT7;

    // Set clock divider to 1 (SMCLK is from DCO at 8 MHz; we'll run SPI at 8 MHz)
    LCD_SPI_BRW = LCD_SPI_DIV;

    // Exit the reset state at the end of the configuration
    LCD_SPI_CTLW0 &= ~UCSWRST;

    // Set CS' (chip select) bit to 0 (display always enabled)
    LCD_CS_OUT &= ~LCD_CS_BIT;
    // D/C idles high (data)
    LCD_DC_OUT |= LCD_DC_BIT;

    return;
}


//*****************************************************************************
// Writes a command to the CFAF128128B-0145T.  This function implements the basic SPI
// interface to the LCD display.
//*****************************************************************************
void HAL_LCD_writeCommand(uint8_t command)
{
    // Let any earlier data byte finish shifting out before touching D/C
    while (!(LCD_SPI_IFG & UCTXIFG));
    while (LCD_SPI_STATW & UCBUSY);
    (void)LCD_SPI_RXBUF;                 // clear stale RX flag

    // D/C low for a command
    LCD_DC_OUT &= ~LCD_DC_BIT;

    LCD_SPI_TXBUF = command;

    // Wait until this byte has been completely clocked out (RXIFG), then D/C high again
    while (!(LCD_SPI_IFG & UCRXIFG));
    (void)LCD_SPI_RXBUF;
    LCD_DC_OUT |= LCD_DC_BIT;
}


//*****************************************************************************
// Writes a data byte to the display (D/C is high between commands).
//*****************************************************************************
void HAL_LCD_writeData(uint8_t data)
{
    // Wait for the transmit buffer to be free
    while (!(LCD_SPI_IFG & UCTXIFG));

    LCD_SPI_TXBUF = data;
}






/* --COPYRIGHT--,BSD
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
