# Tetris on STM32

An embedded implementation of Tetris for the STM32 B-L4S5I-IOT01A Discovery board. Arrange falling pieces on a 10 × 20 grid to clear rows and score points. The playfield and score appear in a serial terminal over UART.

## Controls

- **Tilt the board left or right** to move the falling piece using the onboard accelerometer.
- **Press the user button** to rotate the piece clockwise or restart after game over.

The firmware uses separate FreeRTOS tasks for input, game updates, and UART rendering. A DAC-driven speaker plays a looping Korobeiniki theme during gameplay. Line-clear cues briefly interrupt the theme and resume it at the next note. Game over interrupts playback with a descending chime, then leaves the speaker silent until restart. A single audio task schedules notes without blocking gameplay.
