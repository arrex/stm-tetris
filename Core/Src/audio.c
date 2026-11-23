#include <stdint.h>
#include "arm_math.h"

// 120 MHz internal clk divided by 2720 + 1 cnts/cycle for timer 2
#define SAMPLING_FREQ 44101
// macro for sample cnt
#define SAMPLE_CNT(pitch_hz) ((SAMPLING_FREQ + (pitch_hz) / 2U) / (pitch_hz))

typedef enum  Pitch_Hz {
    A4_HZ = 440,
    C5_HZ = 523,
    E5_HZ = 659,
    C6_HZ = 1047,
    E6_HZ = 1319,
    G6_HZ = 1568,
    C7_HZ = 2093,
} Pitch_Hz;

static uint16_t a4_samples[SAMPLE_CNT(A4_HZ)];
static uint16_t c5_samples[SAMPLE_CNT(C5_HZ)];
static uint16_t e5_samples[SAMPLE_CNT(E5_HZ)];
static uint16_t c6_samples[SAMPLE_CNT(C6_HZ)];
static uint16_t e6_samples[SAMPLE_CNT(E6_HZ)];
static uint16_t g6_samples[SAMPLE_CNT(G6_HZ)];
static uint16_t c7_samples[SAMPLE_CNT(C7_HZ)];

typedef struct Note {
    enum Pitch_Hz pitch_hz;
    uint16_t duration_ms;
} Note;

static const Note tetris_chime[] = {
    {C6_HZ, 60},
    {E6_HZ, 60},
    {G6_HZ, 60},
    {C7_HZ, 140},
};

static const Note game_over_chime[] = {
    {E5_HZ, 120},
    {C5_HZ, 150},
    {A4_HZ, 300},
};

static void init_pitch_samples(uint16_t pitch_buf[], uint32_t sample_cnt) {
    for (int i = 0; i < sample_cnt; i++) {
        float phase = (2 * PI) * (i / (float)sample_cnt);
        pitch_buf[i] = (uint16_t)(1365.0f * (arm_sin_f32(phase) + 1.0f));
    }
}

void audio_init(void) {
    init_pitch_samples(a4_samples, SAMPLE_CNT(A4_HZ));
    init_pitch_samples(c5_samples, SAMPLE_CNT(C5_HZ));
    init_pitch_samples(e5_samples, SAMPLE_CNT(E5_HZ));
    init_pitch_samples(c6_samples, SAMPLE_CNT(C6_HZ));
    init_pitch_samples(e6_samples, SAMPLE_CNT(E6_HZ));
    init_pitch_samples(g6_samples, SAMPLE_CNT(G6_HZ));
    init_pitch_samples(c7_samples, SAMPLE_CNT(C7_HZ));
}
