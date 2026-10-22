# Keegan's Breakout version

MSP430FR5994 LaunchPad + BOOSTXL-EDUMKII.

- `main.c` - the game (joystick paddle, Timer_A0 game loop at 30 fps, collisions, brick counter, 3 lives, buzzer)
- `lcd.c` / `lcd.h` - small SPI driver for the EDUMKII LCD

Controls: joystick left/right moves the paddle, press the joystick or S1 to launch.