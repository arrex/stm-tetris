#include <stdbool.h>
#include <stdint.h>
#include "main.h"
#include "audio.h"
#include "arm_math.h"
#include "cmsis_os.h"

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim2;

// 120 mhz timer clk / (tim2 arr + 1) -> sample freq
#define SAMPLING_FREQ 44101U
#define SAMPLE_CNT(pitch_hz) ((SAMPLING_FREQ + (pitch_hz) / 2U) / (pitch_hz))
#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum {
    REST = 0,
    A5_HZ = 880,
    B5_HZ = 988,
    C6_HZ = 1047,
    D6_HZ = 1175,
    E6_HZ = 1319,
    F6_HZ = 1397,
    G6_HZ = 1568,
    A6_HZ = 1760,
    C7_HZ = 2093,
} Pitch_Hz;

static uint16_t a5_samples[SAMPLE_CNT(A5_HZ)];
static uint16_t b5_samples[SAMPLE_CNT(B5_HZ)];
static uint16_t c6_samples[SAMPLE_CNT(C6_HZ)];
static uint16_t d6_samples[SAMPLE_CNT(D6_HZ)];
static uint16_t e6_samples[SAMPLE_CNT(E6_HZ)];
static uint16_t f6_samples[SAMPLE_CNT(F6_HZ)];
static uint16_t g6_samples[SAMPLE_CNT(G6_HZ)];
static uint16_t a6_samples[SAMPLE_CNT(A6_HZ)];
static uint16_t c7_samples[SAMPLE_CNT(C7_HZ)];

typedef struct {
    Pitch_Hz pitch_hz;
    uint16_t duration_ms;
} Note;

typedef struct {
    const Note *notes;
    uint16_t count;
    bool loop;
} Melody;

// korobeiniki theme -> keep pitches in the same audible range as the chimes
static const Note theme_notes[] = {
    {E6_HZ, 300}, {B5_HZ, 150}, {C6_HZ, 150},
    {D6_HZ, 300}, {C6_HZ, 150}, {B5_HZ, 150},
    {A5_HZ, 300}, {A5_HZ, 150}, {C6_HZ, 150},
    {E6_HZ, 300}, {D6_HZ, 150}, {C6_HZ, 150},
    {B5_HZ, 450}, {C6_HZ, 150}, {D6_HZ, 300}, {E6_HZ, 300},
    {C6_HZ, 300}, {A5_HZ, 300}, {A5_HZ, 450}, {REST, 150},
    {D6_HZ, 450}, {F6_HZ, 150}, {A6_HZ, 300},
    {G6_HZ, 150}, {F6_HZ, 150}, {E6_HZ, 450}, {C6_HZ, 150},
    {E6_HZ, 300}, {D6_HZ, 150}, {C6_HZ, 150},
    {B5_HZ, 300}, {B5_HZ, 150}, {C6_HZ, 150},
    {D6_HZ, 300}, {E6_HZ, 300}, {C6_HZ, 300},
    {A5_HZ, 300}, {A5_HZ, 450}, {REST, 150},
};

static const Note tetris_chime[] = {
    {C6_HZ, 60}, {E6_HZ, 60}, {G6_HZ, 60}, {C7_HZ, 140},
};
static const Note game_over_chime[] = {
    {E6_HZ, 120}, {C6_HZ, 150}, {A5_HZ, 300},
};

static const Melody theme = {theme_notes, ARR_LEN(theme_notes), true};
static const Melody line_clear = {tetris_chime, ARR_LEN(tetris_chime), false};
static const Melody game_over = {game_over_chime, ARR_LEN(game_over_chime), false};

static struct {
    const Melody *melody;
    uint16_t note_index;
    uint16_t theme_resume_index;
    uint32_t deadline_ms;
    bool game_active;
    bool output_running;
} playback;

static int stop_output(void) {
    if (!playback.output_running) {
        return 0;
    }

    HAL_StatusTypeDef timer_status = HAL_TIM_Base_Stop(&htim2);
    HAL_StatusTypeDef dac_status = HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);

    playback.output_running = false;

    return timer_status == HAL_OK && dac_status == HAL_OK ? 0 : -1;
}

static void init_pitch_samples(uint16_t samples[], unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        float phase = (2 * PI) * (i / (float)count);
        samples[i] = (uint16_t)(1365.0f * (arm_sin_f32(phase) + 1.0f));
    }
}

static uint16_t *samples_for_pitch(Pitch_Hz pitch_hz) {
    switch (pitch_hz) {
        case A5_HZ: return a5_samples;
        case B5_HZ: return b5_samples;
        case C6_HZ: return c6_samples;
        case D6_HZ: return d6_samples;
        case E6_HZ: return e6_samples;
        case F6_HZ: return f6_samples;
        case G6_HZ: return g6_samples;
        case A6_HZ: return a6_samples;
        case C7_HZ: return c7_samples;
        default: return NULL;
    }
}

static int start_note(const Note *note) {
    if (stop_output() != 0) {
        return -1;
    }

    if (note->pitch_hz == REST) {
        return 0;
    }

    uint16_t *samples = samples_for_pitch(note->pitch_hz);
    if (samples == NULL) {
        return -1;
    }

    if (HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)samples,
                          SAMPLE_CNT(note->pitch_hz), DAC_ALIGN_12B_R) != HAL_OK) {
        HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        return -1;
    }

    playback.output_running = true;
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK) {
        stop_output();
        return -1;
    }

    return 0;
}

static int begin_current_note(uint32_t now_ms) {
    const Note *note = &playback.melody->notes[playback.note_index];
    if (start_note(note) != 0) {
        return -1;
    }

    playback.deadline_ms = now_ms + note->duration_ms;

    return 0;
}

static int select_melody(const Melody *melody, uint16_t index, uint32_t now_ms) {
    playback.melody = melody;
    playback.note_index = index;
    return begin_current_note(now_ms);
}

int audio_handle_event(uint32_t events, uint32_t now_ms) {
    if (events & AUDIO_START) {
        playback.game_active = true;
        playback.theme_resume_index = 0;
        // restart & game over in the same iteration -> game over takes priority
        if (!(events & AUDIO_GAME_OVER) &&
            select_melody(&theme, 0, now_ms) != 0) {
            return -1;
        }
    }

    if (events & AUDIO_GAME_OVER) {
        if (!playback.game_active) {
            return 0;
        }
        playback.game_active = false;
        return select_melody(&game_over, 0, now_ms);
    }

    if ((events & AUDIO_LINE_CLEAR) && playback.game_active &&
        playback.melody == &theme) {
        playback.theme_resume_index = (playback.note_index + 1U) % theme.count;
        return select_melody(&line_clear, 0, now_ms);
    }

    return 0;
}

uint32_t audio_wait_ms(uint32_t now_ms) {
    if (playback.melody == NULL) {
        return osWaitForever;
    }

    // signed diff -> handle tick wraparound for these short durations
    int32_t remaining = (int32_t)(playback.deadline_ms - now_ms);

    return remaining > 0 ? (uint32_t)remaining : 0;
}

int audio_update(uint32_t now_ms) {
    if (playback.melody == NULL || audio_wait_ms(now_ms) != 0) {
        return 0;
    }

    ++playback.note_index;
    if (playback.note_index < playback.melody->count) {
        return begin_current_note(now_ms);
    }

    if (playback.melody->loop) {
        return select_melody(playback.melody, 0, now_ms);
    }

    if (playback.game_active) {
        return select_melody(&theme, playback.theme_resume_index, now_ms);
    }

    playback.melody = NULL;

    return stop_output();
}

int audio_init(void) {
    if (stop_output() != 0) {
        return -1;
    }

    playback.melody = NULL;
    playback.game_active = false;

    init_pitch_samples(a5_samples, ARR_LEN(a5_samples));
    init_pitch_samples(b5_samples, ARR_LEN(b5_samples));
    init_pitch_samples(c6_samples, ARR_LEN(c6_samples));
    init_pitch_samples(d6_samples, ARR_LEN(d6_samples));
    init_pitch_samples(e6_samples, ARR_LEN(e6_samples));
    init_pitch_samples(f6_samples, ARR_LEN(f6_samples));
    init_pitch_samples(g6_samples, ARR_LEN(g6_samples));
    init_pitch_samples(a6_samples, ARR_LEN(a6_samples));
    init_pitch_samples(c7_samples, ARR_LEN(c7_samples));

    return 0;
}
