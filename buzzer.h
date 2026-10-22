/*
 * buzzer.h - Non-blocking sound effects on the BoosterPack MK II piezo buzzer
 * (J4.40 = P3.7 = TB0.6 on the FR5994 LaunchPad).
 *
 * Timer B0 makes the tone (PWM on TB0.6). Timer A1 is a 10 ms tick that steps
 * through the notes, so the game never waits for a sound to finish.
 */
#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdint.h>

typedef struct {
    uint16_t freq;      // Hz, 0 = rest (silence)
    uint16_t ms;        // duration in milliseconds (10 ms resolution)
} Note;

void Buzzer_Init(void);
void Buzzer_GpioTest(void);   // 1 s, 4 kHz bit-banged square wave (no timer needed)
void Buzzer_Off(void);

// Play a sequence of notes. A new sequence only interrupts a running one
// if its priority is equal or higher.
void Buzzer_PlaySequence(const Note *notes, uint8_t count, uint8_t priority);

// Ready-made game sounds
void Buzzer_PlayBlockHit(void);   // short blip
void Buzzer_PlayLoseLife(void);   // two falling notes
void Buzzer_PlayGameOver(void);   // longer falling tune
void Buzzer_PlayWin(void);        // rising fanfare

#endif /* BUZZER_H_ */
