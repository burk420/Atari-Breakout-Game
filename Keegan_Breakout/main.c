// main.c - ECE 4650 Embedded Design Project: Atari Breakout on the MSP430
// MSP-EXP430FR5994 LaunchPad + BOOSTXL-EDUMKII
//
// HOW IT WORKS
//   Timer_A0 wakes the CPU 30 times a second. Each wake-up is one frame:
//   read the joystick (ADC), move the paddle, move the ball, check for
//   collisions, and redraw. Timer_B0 makes the buzzer beep (PWM).
//   The CPU sleeps in LPM0 between frames.
//
// CONTROLS
//   Joystick left/right = move the paddle.  Press the joystick or S1 = launch.
//   40 bricks, 3 lives, beep on every hit.

#include <msp430.h>
#include <stdint.h>
#include "lcd.h"              // the screen driver (talks to the LCD over SPI)

// ---- sizes on the 128 x 128 pixel screen ----
#define ROWS     5          // bricks: 5 rows x 8 columns = 40 bricks
#define COLS     8
#define BRICK_W  14         // each brick is 14 x 5 pixels, 16 pixels apart
#define BRICK_H  5
#define TOP      14         // y of the first row of bricks
#define PAD_W    24         // paddle is 24 x 3 pixels
#define PAD_H    3
#define PAD_Y    120        // paddle's y position (near the bottom)
#define BALL     3          // ball is 3 x 3 pixels

const uint16_t rowColor[ROWS] = {RED, YELLOW, GREEN, CYAN, BLUE};

// ---- game variables ----
uint8_t bricks[ROWS];       // one byte per row, one bit per brick (1 = still there)
int bricksLeft;             // BRICK COUNTER: starts at 40, at 0 you win
int lives, score;
int padX;                   // paddle x position
int ballX, ballY;           // ball position
int dx, dy;                 // ball speed: pixels moved each frame
int onPaddle;               // 1 = ball is sitting on the paddle waiting to launch
volatile int tick;          // set to 1 by Timer_A0 every 1/30 s
volatile int beep;          // how many more frames to beep (0 = quiet)

// =================================================================== setup
void setup(void)
{
    // stop the watchdog, otherwise it would keep resetting the board
    WDTCTL = WDTPW | WDTHOLD;

    // CLOCK: run MCLK and SMCLK at 8 MHz.
    // (TI's recommended way: divide by 4 while switching, then go full speed)
    FRCTL0 = FRCTLPW | NWAITS_1;
    CSCTL0_H = CSKEY_H;
    CSCTL3 = DIVA__4 | DIVS__4 | DIVM__4;
    CSCTL1 = DCOFSEL_3 | DCORSEL;              // DCO = 8 MHz
    __delay_cycles(60);
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;      // MCLK = SMCLK = 8 MHz
    CSCTL0_H = 0;

    // ADC (joystick)
    //   Joystick X is on pin P1.2 = analog channel A2.
    //   12-bit result: 0 to 4095. Reference = 3.3 V (default).
    //   Center of the joystick reads about 2048.
    P1SEL0 |= BIT2;  P1SEL1 |= BIT2;           // P1.2 -> analog input A2
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;         // sample time 16 cycles, ADC on
    ADC12CTL1 = ADC12SHP;                      // use the sample timer
    ADC12CTL2 = ADC12RES_2;                    // 12-bit
    ADC12MCTL0 = ADC12INCH_2;                  // read channel A2

    // launch buttons: joystick press = P6.2, S1 = P4.3
    // pull-up resistors on, so the pin reads 1 normally and 0 when pressed
    P6REN |= BIT2;  P6OUT |= BIT2;
    P4REN |= BIT3;  P4OUT |= BIT3;

    // buzzer on P3.7 (output)
    P3DIR |= BIT7;

    PM5CTL0 &= ~LOCKLPM5;                      // unlock the I/O pins

    // TIMER_A0 = GAME LOOP
    //   SMCLK 8 MHz / 8 = 1 MHz.  Up mode: counts 0 to CCR0, then restarts.
    //   1 MHz / 33333 = 30 interrupts a second = 30 frames a second.
    TA0CCR0 = 33333;
    TA0CCTL0 = CCIE;                           // interrupt when it reaches CCR0
    TA0CTL = TASSEL__SMCLK | ID__8 | MC__UP | TACLR;

    // TIMER_B0 = BUZZER (PWM)
    //   CCR0 = 4000 -> 8 MHz / 4000 = 2 kHz beep
    //   CCR1 = 200  -> buzzer is on 200 / 4000 = 5% of the time (keeps it quiet)
    //   CCR0 interrupt turns the buzzer on, CCR1 interrupt turns it off.
    //   Bigger CCR1 = louder.
    TB0CCR0 = 4000;
    TB0CCR1 = 200;
    TB0CCTL0 = CCIE;
    TB0CCTL1 = CCIE;
    TB0CTL = TBSSEL__SMCLK | MC__UP | TBCLR;

    lcd_init();                                // start the screen (SPI)
    __enable_interrupt();
}

// read the joystick: one single conversion, started by software
int joystick(void)
{
    ADC12CTL0 |= ADC12ENC | ADC12SC;           // start a conversion
    while (ADC12CTL1 & ADC12BUSY) ;            // wait until it's done
    return ADC12MEM0;                          // 0 to 4095
}

// =================================================================== drawing
// top line of the screen, e.g. "LIVES:3  SCORE:12"
void drawScore(void)
{
    char s[] = "LIVES:3  SCORE:00";
    s[6]  = '0' + lives;
    s[15] = '0' + score / 10;
    s[16] = '0' + score % 10;
    lcd_text(13, 2, s, WHITE);
}

// clear the screen, draw all 40 bricks, reset lives and score
void newGame(void)
{
    int r, c;
    lcd_rect(0, 0, 128, 128, BLACK);
    for (r = 0; r < ROWS; r++) {
        bricks[r] = 0xFF;                      // 0xFF = all 8 bits on = all 8 bricks
        for (c = 0; c < COLS; c++)
            lcd_rect(c * 16 + 1, TOP + r * 7, BRICK_W, BRICK_H, rowColor[r]);
    }
    bricksLeft = ROWS * COLS;                  // 40
    lives = 3;
    score = 0;
    onPaddle = 1;                              // ball starts on the paddle
    drawScore();
}

// =================================================================== ball + collisions
void moveBall(void)
{
    int cx, cy, row, col, hit;

    ballX += dx;                               // move the ball
    ballY += dy;

    // WALLS: if the ball reaches an edge, flip its direction
    if (ballX <= 0)          { ballX = 0;          dx = -dx; }   // left wall
    if (ballX >= 128 - BALL) { ballX = 128 - BALL; dx = -dx; }   // right wall
    if (ballY <= 11)         { ballY = 11;         dy = -dy; }   // top (under the score)

    // PADDLE: rectangle overlap check (is the ball touching the paddle?)
    if (dy > 0 && ballY + BALL >= PAD_Y && ballY + BALL <= PAD_Y + PAD_H &&
        ballX + BALL > padX && ballX < padX + PAD_W) {
        ballY = PAD_Y - BALL;
        dy = -dy;                              // bounce up
        // where it hits sets the angle: edges = sharp (dx = 2), middle = shallow (dx = 1)
        hit = ballX + 1 - padX;                // 0 (left end) to 23 (right end)
        if      (hit < 6)  dx = -2;
        else if (hit < 12) dx = -1;
        else if (hit < 18) dx = 1;
        else               dx = 2;
        beep = 2;
    }

    // BRICKS: find the brick under the middle of the ball
    //   row = (y - 14) / 7    col = x / 16
    cx = ballX + 1;
    cy = ballY + 1;
    row = (cy - TOP) / 7;
    col = cx / 16;
    if (cy >= TOP && row < ROWS && (bricks[row] & (1 << col))) {   // brick still there?
        bricks[row] &= ~(1 << col);            // clear its bit = remove the brick
        lcd_rect(col * 16 + 1, TOP + row * 7, BRICK_W, BRICK_H, BLACK);   // erase it
        dy = -dy;                              // bounce
        bricksLeft--;                          // brick counter goes down
        score++;
        beep = 3;
        drawScore();
        if (bricksLeft == 0) newGame();        // all 40 gone = you win, start over
    }

    // BOTTOM: missed the ball -> lose a life
    if (ballY >= 125) {
        lives--;
        beep = 10;
        onPaddle = 1;                          // put the ball back on the paddle
        if (lives == 0) newGame();             // out of lives = game over, reset
        else drawScore();
    }
}

// =================================================================== main loop
int main(void)
{
    int speed;

    setup();
    newGame();

    while (1) {
        // sleep in LPM0 until Timer_A0 wakes us up (30 times a second).
        // LPM0 (not LPM3) because both timers run on SMCLK, and LPM3 turns SMCLK off.
        __bis_SR_register(LPM0_bits | GIE);
        if (!tick) continue;
        tick = 0;
        if (beep) beep--;                      // count down the beep

        // PADDLE: push the joystick to slide it, further = faster.
        //   (reading - 2048) / 256 gives -8 to +7 pixels per frame.
        //   Joystick in the middle = 0 = paddle stays still.
        speed = (joystick() - 2048) / 256;
        if (speed != 0) {
            lcd_rect(padX, PAD_Y, PAD_W, PAD_H, BLACK);   // erase old paddle
            padX += speed;
            if (padX < 0) padX = 0;                       // stop at the left edge
            if (padX > 128 - PAD_W) padX = 128 - PAD_W;   // stop at the right edge
        }
        lcd_rect(padX, PAD_Y, PAD_W, PAD_H, WHITE);       // draw paddle

        // BALL: only redraw what moved: erase the old ball, then draw the new one
        lcd_rect(ballX, ballY, BALL, BALL, BLACK);
        if (onPaddle) {
            // LAUNCH POSITION: ball sits on the middle of the paddle
            ballX = padX + PAD_W / 2 - 1;
            ballY = PAD_Y - BALL;
            dx = 1;
            dy = -2;
            if (!(P6IN & BIT2) || !(P4IN & BIT3)) onPaddle = 0;   // button pressed -> launch
        } else {
            moveBall();
        }
        lcd_rect(ballX, ballY, BALL, BALL, WHITE);
    }
}

// =================================================================== interrupts
// Timer_A0 CCR0: 30 times a second. Starts one frame of the game.
#pragma vector = TIMER0_A0_VECTOR
__interrupt void gameTimer(void)
{
    tick = 1;
    __bic_SR_register_on_exit(LPM0_bits);      // wake up main()
}

// Timer_B0 CCR0: start of each 2 kHz period -> buzzer on (only while beeping)
#pragma vector = TIMER0_B0_VECTOR
__interrupt void buzzerOn(void)
{
    if (beep) P3OUT |= BIT7;
}

// Timer_B0 CCR1: 5% into the period -> buzzer off
#pragma vector = TIMER0_B1_VECTOR
__interrupt void buzzerOff(void)
{
    TB0CCTL1 &= ~CCIFG;
    P3OUT &= ~BIT7;
}
