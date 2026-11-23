#include <stdint.h>
#include "arm_math.h"

// 120 MHz internal clk divided by 2720 cnts/cycle for timer 2
#define SAMPLING_FREQ 44118

typedef enum Pitch {
    A4,
    C5,
    E5,
    C6,
    E6,
    G6,
    C7,
} Pitch;

typedef struct Note {
    enum Pitch pitch;
    uint16_t duration_ms;
} Note;

static const uint16_t pitch_to_hz[] = {
    [A4] = 440,
    [C5] = 523,
    [E5] = 659,
    [C6] = 1047,
    [E6] = 1319,
    [G6] = 1568,
    [C7] = 2093,
};

static uint16_t samples[256];

static const Note tetris_chime[] = {
    {C6, 60},
    {E6, 60},
    {G6, 60},
    {C7, 140},
};

static const Note game_over_chime[] = {
    {E5, 120},
    {C5, 150},
    {A4, 300},
};

static void init_pitch_samples(enum Pitch pitch, uint16_t pitch_buf[]) {
    uint16_t pitch_hz = pitch_to_hz[pitch];
    uint32_t sample_cnt = ((SAMPLING_FREQ + pitch_hz) / 2) / pitch_hz;

    for (int i = 0; i < sample_cnt; i++) {
        float phase = (2 * PI) * (i / (float)sample_cnt);
        pitch_buf[i] = (uint16_t)(1365.0f * (arm_sin_f32(phase) + 1.0f));
    }
}
