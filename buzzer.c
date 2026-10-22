/*
 * buzzer.c - Piezo buzzer sound effects (see buzzer.h)
 *
 * Tone:  Timer B0, SMCLK / 8 = 1 MHz, up mode. TB0CCR0 sets the period,
 *        TB0CCR6 = half of it (50% duty), output mode reset/set on TB0.6.
 * Tick:  Timer A1, SMCLK / 8 = 1 MHz, CCR0 interrupt every 10 ms advances the notes.
 */
#include "board.h"
#include "buzzer.h"

#ifdef BUZZER_ENABLED

#define TONE_CLOCK_HZ   1000000UL   // SMCLK (8 MHz) / 8
#define TICK_MS         10

// Priorities: higher number wins
#define PRIO_HIT   1
#define PRIO_LIFE  2
#define PRIO_END   3

static const Note *seq = 0;
static uint8_t     seqLen = 0, seqIdx = 0, curPrio = 0;
static volatile uint16_t msLeft = 0;

static const Note SND_HIT[]      = { {3000, 30} };
static const Note SND_LOSE_LIFE[] = { {1200, 120}, {900, 200} };
static const Note SND_GAME_OVER[] = { {1600, 150}, {1300, 150}, {1000, 150}, {700, 450} };
static const Note SND_WIN[]      = { {2000, 100}, {2500, 100}, {3000, 100}, {4000, 300} };

static void setTone(uint16_t freq)
{
    if (freq == 0) {
        BUZZER_TB_CTL   = TBCLR;          // stop timer
        BUZZER_TB_CCTL  = 0;              // OUT = 0, pin driven low
    } else {
        uint16_t period = (uint16_t)(TONE_CLOCK_HZ / freq);
        BUZZER_TB_CCR0  = period - 1;
        BUZZER_TB_CCR   = period / 2;
        BUZZER_TB_CCTL  = OUTMOD_7;       // reset/set
        BUZZER_TB_CTL   = TBSSEL_2 | ID_3 | MC_1 | TBCLR;
    }
}

static void nextNote(void)
{
    if (seq != 0 && seqIdx < seqLen) {
        Note n = seq[seqIdx++];
        setTone(n.freq);
        msLeft = n.ms;
    } else {
        setTone(0);
        seq = 0;
        msLeft = 0;
        curPrio = 0;
    }
}

// Pin/buzzer check without any timer: plain GPIO square wave, 4 kHz for 1 second
void Buzzer_GpioTest(void)
{
    uint16_t i;
    BUZZER_SEL0 &= ~BUZZER_BIT;
    BUZZER_SEL1 &= ~BUZZER_BIT;
    BUZZER_DIR  |=  BUZZER_BIT;
    for (i = 0; i < 8000; i++) {
        BUZZER_OUT ^= BUZZER_BIT;
        __delay_cycles(MCLK_HZ / 8000);   // half period of 4 kHz at 8 MHz = 1000 cycles
    }
    BUZZER_OUT &= ~BUZZER_BIT;
}

void Buzzer_Init(void)
{
    // P3.7 -> TB0.6 output: direction must be OUTPUT (PxDIR = 1 selects the compare output
    // rather than the capture input), primary module function: SEL1 = 0, SEL0 = 1
    BUZZER_DIR  |=  BUZZER_BIT;
    BUZZER_SEL1 &= ~BUZZER_BIT;
    BUZZER_SEL0 |=  BUZZER_BIT;
    setTone(0);

    // 10 ms tick on Timer A1
    TA1CCR0  = (TONE_CLOCK_HZ * TICK_MS / 1000) - 1;
    TA1CCTL0 = CCIE;
    TA1CTL   = TASSEL_2 | ID_3 | MC_1 | TACLR;
}

void Buzzer_Off(void)
{
    seq = 0; msLeft = 0; curPrio = 0;
    setTone(0);
}

void Buzzer_PlaySequence(const Note *notes, uint8_t count, uint8_t priority)
{
    if (seq != 0 && priority < curPrio) return;   // don't cut off a more important sound
    seq = notes;
    seqLen = count;
    seqIdx = 0;
    curPrio = priority;
    nextNote();
}

void Buzzer_PlayBlockHit(void) { Buzzer_PlaySequence(SND_HIT, 1, PRIO_HIT); }
void Buzzer_PlayLoseLife(void) { Buzzer_PlaySequence(SND_LOSE_LIFE, 2, PRIO_LIFE); }
void Buzzer_PlayGameOver(void) { Buzzer_PlaySequence(SND_GAME_OVER, 4, PRIO_END); }
void Buzzer_PlayWin(void)      { Buzzer_PlaySequence(SND_WIN, 4, PRIO_END); }

#pragma vector = TIMER1_A0_VECTOR
__interrupt void BUZZER_TICK(void)
{
    if (msLeft != 0) {
        if (msLeft <= TICK_MS) {
            msLeft = 0;
            nextNote();
        } else {
            msLeft -= TICK_MS;
        }
    }
}

#else  /* BUZZER_ENABLED not defined: all sounds compile to nothing */

void Buzzer_Init(void) {}
void Buzzer_GpioTest(void) {}
void Buzzer_Off(void) {}
void Buzzer_PlaySequence(const Note *notes, uint8_t count, uint8_t priority) {}
void Buzzer_PlayBlockHit(void) {}
void Buzzer_PlayLoseLife(void) {}
void Buzzer_PlayGameOver(void) {}
void Buzzer_PlayWin(void) {}

#endif
