/*
 * Breakout
 *
 * It's a simple clone of the Atari "Breakout" classic. Use the joystick on the
 *  educational booster pack to move left and right, bouncing the ball up into
 *  the field of bricks. Break all of them to move to the next level.
 *
 * If the ball falls off the south edge of the playing field, press the button (J4.32)
 *  to send it back out. You have three lives, but they are replenished every five levels.
 *
 * Future Goals:
 *   - Requires a level editor
 * - Power-ups
 *   - "Punching": ball doesn't bounce off blocks, but goes through them
 * - Sound effects maybe? Make use of the buzzer for simple "bzz" on hits
 */

#include "board.h"
#include "Grlib/grlib/grlib.h"      // Graphics Library Import
#include "LcdDriver/lcd_driver.h"
#include "LcdDriver/middle_driver.h"
#include "graphics.h"
#include "math_utils.h"
#include "levels.h"
#include <stdio.h>
#include <string.h>

Graphics_Context g_sContext;      // Declare our graphics context for the library
Level* level;
Graphics_Rectangle blocks[30];
int32_t* palette;
char* bindings;

unsigned char numBlocks = 30;
const Graphics_Rectangle nullBlock = { 128, 128, 128, 128 };

unsigned int paddle_x = 1;

int lives = 3;
int levelNum = 0;
unsigned int points = 0;
unsigned char activeBlocks = 30;

#define redLED LED_BIT
#define BUTTON BTN_BIT
#define circleRadius 2

void initializeADC(void);
int readADC(unsigned int*data);

int main(void)
{
    // Standard MSP430 Behavior
	WDTCTL   =  WDTPW | WDTHOLD;	  // Stop Watchdog Timer
	PM5CTL0 &= ~LOCKLPM5;             // Enable GPIO

	// Configure DCO = 8MHz, MCLK = SMCLK = DCO / 1 (8MHz) for SPI and the update timer.
	// FR5994 has no FRAM wait-state requirement at 8MHz (FRCTL0 only needed above 8MHz).
	CSCTL0_H =  CSKEY_H;                // Unlock CS registers
	CSCTL1   =  DCOFSEL_6;            // DCO = 8MHz
	CSCTL2   =  SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
	CSCTL3   =  DIVA__1 | DIVS__1 | DIVM__1;
	CSCTL0_H =  0;                    // Lock CS registers

	// Configure debug LED
	LED_DIR |=  redLED;
	LED_OUT &= ~redLED;

	// Configure Button Input
	BTN_DIR &= ~BUTTON;
	BTN_REN |=  BUTTON;
	BTN_OUT |=  BUTTON;
	BTN_IES |=  BUTTON;

	initializeADC();

	/////////////////////////////////////////////////////
	//           Initialize graphics library           //
	/////////////////////////////////////////////////////
#ifndef LCD_PINSCAN
	Crystalfontz128x128_Init();       // Initialize our display communication

	Initialize_Graphics(&g_sContext); // Prepare the display with initial commands
#endif

#ifdef LCD_PINSCAN
	// Pin scan (see PORTING.md): for each candidate D/C pin, blink the LED (index+1) times,
	// try to init the LCD, then show red/green/blue. Note the blink count when colors appear.
	for (;;) {
	    unsigned char idx, n;
	    for (idx = 0; idx < LCD_ScanCount; idx++) {
	        for (n = 0; n <= idx; n++) { LED_OUT |= redLED; DELAY_MS(200); LED_OUT &= ~redLED; DELAY_MS(200); }
	        DELAY_MS(600);
	        LCD_PinScanSelect(idx);
	        Crystalfontz128x128_Init();
	        Initialize_Graphics(&g_sContext);
	        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_RED);   Graphics_clearDisplay(&g_sContext); DELAY_MS(900);
	        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_GREEN); Graphics_clearDisplay(&g_sContext); DELAY_MS(900);
	        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLUE);  Graphics_clearDisplay(&g_sContext); DELAY_MS(900);
	    }
	}
#endif

#ifdef LCD_SELFTEST
	// LCD-only test (see board.h).
	// Step 1: blink the backlight 3 times (backlight pin + power + J5 jumper test)
	{
	    unsigned char n;
	    for (n = 0; n < 3; n++) {
	        LCD_BL_OUT &= ~LCD_BL_BIT; DELAY_MS(400);
	        LCD_BL_OUT |=  LCD_BL_BIT; DELAY_MS(400);
	    }
	}
	// Step 2: cycle solid colors forever
	for (;;) {
	    static const uint32_t testColors[4] = { GRAPHICS_COLOR_RED, GRAPHICS_COLOR_GREEN, GRAPHICS_COLOR_BLUE, GRAPHICS_COLOR_WHITE };
	    unsigned char k;
	    for (k = 0; k < 4; k++) {
	        Graphics_setBackgroundColor(&g_sContext, testColors[k]);
	        Graphics_clearDisplay(&g_sContext);
	        LED_OUT ^= redLED;
	        DELAY_MS(700);
	    }
	}
#endif

	// Load our level
	level = &LEVELS[levelNum];
	memcpy(blocks, level->blocks, sizeof(Graphics_Rectangle) * 30);
	palette = level->colors;
	bindings = level->bindings;
	activeBlocks = level->numBlocks;
	numBlocks = level->numBlocks;

	Draw_Playspace(&g_sContext, blocks, palette, bindings, numBlocks, lives, levelNum);

	/////////////////////////////////////////////////////
	//               Initialize update timer           //
	/////////////////////////////////////////////////////
	// Use SMCLK @8KHz, Divide by 8, in up mode. Clear it, enable the interrupt flag
	TA0CTL  =  TASSEL_2 | ID_3 | MC_1 | TACLR | TAIE;
	TA0CCR0 =  33333 - 1;             // Appx. 30.003Hz, or, 33.33ms
	TA0CTL &= ~TAIFG;                 // Clear interrupt flag

	// Go to sleep
	_low_power_mode_1();

	return 0;
}

#pragma vector = TIMER0_A1_VECTOR
__interrupt void FIXED_UPDATE(void) {
    static int velocity_x = 1, velocity_y = 2;
    static int pos_x = 64, pos_y = 64;
    static char isFree = 0;
    unsigned char i;

    // Clear interrupt flag
    TA0CTL &= ~TAIFG;

    // Set light
    LED_OUT ^= redLED;

    // Paddle Logic
    Graphics_Rectangle paddle_rect = {
                                      paddle_x, PADDLE_Y, paddle_x + PADDLE_WIDTH, PADDLE_Y + PADDLE_HEIGHT
    };

    Draw_Paddle(&g_sContext, paddle_x, GRAPHICS_COLOR_BLACK);

    readADC(&paddle_x);
    paddle_x = (paddle_x / 4095.0) * 128 - PADDLE_WIDTH;

    paddle_rect.xMin = paddle_x;
    paddle_rect.xMax = paddle_x + PADDLE_WIDTH;

    if(isFree) {
        // Calculate our new position
        int temp_pos_x = pos_x, temp_pos_y = pos_y;
        temp_pos_x += velocity_x;
        temp_pos_y += velocity_y;

        // Create our bounds for our circle
        Graphics_Rectangle circleBounds = {
                                           temp_pos_x - circleRadius,
                                           temp_pos_y - circleRadius,
                                           temp_pos_x + circleRadius,
                                           temp_pos_y + circleRadius
        };

        // Check if circle collides with walls
        char isCollidingWalls = IsCollidingWalls(&circleBounds);

        if((isCollidingWalls & COLLIDING_SOUTH) != 0) {
            // We've hit the bottom of the map! Set our next frame to be locked to the paddle,
            // take a life
            isFree = 0;

            lives--;
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillCircle(&g_sContext, (lives*6)+4, 123, circleRadius);

            // Check for Loss Condition
            if(lives < 0) {
                // We're dead. Stop the game, and establish the reset button's interrupt
                Draw_ModalBox(&g_sContext, "Game Over!", "Play Again?", 1, points);

                // Turn off the timer
                TA0CTL &= ~MC_3;

                // Enable Interrupt
                BTN_IE  |=  BUTTON;
                BTN_IFG &= ~BUTTON;

                // Break out of this
                return;
            }
        }

        // Check if circle is colliding with our blocks
        for(i = 0; i < numBlocks; i++) {
            if(IsNullBlock(&blocks[i])) continue;

            char isCollidingBlock = IsCollidingAABB(&blocks[i], &circleBounds);
            isCollidingWalls |= isCollidingBlock;

            if(isCollidingBlock) {
                Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
                Graphics_fillRectangle(&g_sContext, &blocks[i]);
                blocks[i] = nullBlock;
                activeBlocks--;

                points++;

                // Check for Win Condition
                if(activeBlocks == 0) {
                    // We won! Stop the game, and establish the reset button's interrupt
                    Draw_ModalBox(&g_sContext, "You Win!", "Continue?", 1, points);

                    // Capture the ball
                    isFree = 0;

                    // Turn off the timer
                    TA0CTL &= ~MC_3;

                    // Enable Interrupt
                    BTN_IE  |=  BUTTON;
                    BTN_IFG &= ~BUTTON;

                    // Break out of this
                    return;
                }
            }
        }

        char isCollidingPaddle = IsCollidingAABB(&circleBounds, &paddle_rect) & ~(COLLIDING_EAST | COLLIDING_WEST);
        isCollidingWalls |= isCollidingPaddle;

        // Clear our old position
        Draw_Ball(&g_sContext, pos_x, pos_y, GRAPHICS_COLOR_BLACK);

        // Our circle is colliding
        if(isCollidingWalls != 0) {
            // Find the colliding direction, flipping velocity
            if((isCollidingWalls & (COLLIDING_EAST | COLLIDING_WEST)) != 0)
                velocity_x = -velocity_x;
            if((isCollidingWalls & (COLLIDING_NORTH | COLLIDING_SOUTH)) != 0)
                velocity_y = -velocity_y;

            // Apply the new vectors
            pos_x += velocity_x;
            pos_y += velocity_y;
        } else {
            // These old calcs are good, use them.
            pos_x = temp_pos_x;
            pos_y = temp_pos_y;
        }
    } else {
        // We're not free - we're to be locked to the top of the paddle

        // Clear our old position
        Draw_Ball(&g_sContext, pos_x, pos_y, GRAPHICS_COLOR_BLACK);

        // Place us above the paddle
        pos_x = paddle_x + PADDLE_WIDTH / 2;
        pos_y = PADDLE_Y - 4;

        // If the user has the release button down
        if((BTN_IN & BUTTON) == 0) {
            // Set the ball free
            isFree = 1;
        }
    }

    Draw_Paddle(&g_sContext, paddle_x, GRAPHICS_COLOR_RED);

    // Redrawing of Circle
    if(lives >= 0) {
        Draw_Ball(&g_sContext, pos_x, pos_y, GRAPHICS_COLOR_WHITE);
    }
}

#pragma vector = BTN_VECTOR
__interrupt void RESET_ISR() {
    // Clear flag, disable interrupt
    BTN_IFG &= ~BUTTON;
    BTN_IE  &= ~BUTTON;

    if(lives < 0) {
        points = 0; // Reset points if loss
        lives  = 3; // Reset lives if loss
        levelNum = 0;
    } else {
        levelNum++;             // Level-up

        // Every five levels, we reset the user to three lives
        if(levelNum % 5 == 0) {
            lives = 3;
        }
    }

    // Set lives to max
    lives = 3;

    // Rest our active block count
    activeBlocks = 30;

    // Clear our screen, reset everything to zero.
    level = &LEVELS[levelNum % NUM_LEVELS];
    memcpy(blocks, level->blocks, sizeof(Graphics_Rectangle) * 30);
    palette = level->colors;
    bindings = level->bindings;
    numBlocks = level->numBlocks;
    activeBlocks = level->numBlocks;

    // Draw the original screen
    Draw_Playspace(&g_sContext, blocks, palette, bindings, numBlocks, lives, levelNum);

    // Begin play loop
    TA0CTL |= MC_1;
}

void initializeADC(void) {
    // Divert pins for analong functionality
    JOY_SEL1 |= JOY_BIT;
    JOY_SEL0 |= JOY_BIT;

    // Turn on the ADC module
    ADC12CTL0 |= ADC12ON;

    // Turn off the ENC (Enable Conversion) bit while modifying the config
    ADC12CTL0 &= ~ADC12ENC;

    // Set ADC12SHT0 (Determined number of cycles)
    ADC12CTL0 |= BIT8 | BIT9;

    // *********** ADC12CTL1 ******** //
    // Set ADC12SHS (select ADC12SC bit as the trigger)
    // Set ADC12SHP bit
    // Set ADC12DIV (select the divider you determined)
    // Set ADC12SSEL (select MODOSC)
    // Looks like only bit 9 needs to be set high.
    ADC12CTL1 = BIT9;

    // *********** ADC12CTL2 ******** //
    // Set ADC12RES (select 12-bit resolution)
    // [5:4] 10b
    // Set ADC12DF  (select unsigned binary format)
    // [3] 0b
    ADC12CTL2 = BIT5;

    // *********** ADC12CTL3 ******** //
    // Leave all fields default

    // *********** ADC12MCTL0 ******** //
    // Set ADC12VRSEL (select VR+=AVCC, VR-=AVSS)
    // [11:8] 0000b
    // Set ADC12INCH  (channel of the joystick X pin, see board.h)
    ADC12MCTL0 = JOY_INCH;

    // Turn on ENC to signal the end of configuration
    ADC12CTL0 |= ADC12ENC;

    return;
}

int readADC(unsigned int* data) {
    // Set ADC12SC bit
    ADC12CTL0 |= BIT0;
    // Wait for ADC12Busy bit to clear
    while((ADC12CTL1 & BIT0) != 0) {}

    // Read result from register ADC12MEM0
    (*data) = ADC12MEM0;

    return 1;
}
