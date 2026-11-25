#include <stdint.h>
#include "main.h"
#include "audio.h"
#include "arm_math.h"
#include "cmsis_os.h"

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim2;

// 120 MHz internal clk divided by 2720 + 1 cnts/cycle for timer 2
#define SAMPLING_FREQ 44101
// macro for sample cnt
#define SAMPLE_CNT(pitch_hz) ((SAMPLING_FREQ + (pitch_hz) / 2U) / (pitch_hz))
// macro to compute arr len
#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum  Pitch_Hz {
    A5_HZ = 880,
    C6_HZ = 1047,
    E6_HZ = 1319,
    G6_HZ = 1568,
    C7_HZ = 2093,
} Pitch_Hz;

static uint16_t a5_samples[SAMPLE_CNT(A5_HZ)];
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

// Keep the descending melody in a higher register for audibility.
static const Note game_over_chime[] = {
    {E6_HZ, 120},
    {C6_HZ, 150},
    {A5_HZ, 300},
};

static void init_pitch_samples(uint16_t pitch_buf[], int sample_cnt) {
    for (int i = 0; i < sample_cnt; i++) {
        float phase = (2 * PI) * (i / (float)sample_cnt);
        pitch_buf[i] = (uint16_t)(1365.0f * (arm_sin_f32(phase) + 1.0f));
    }
}

static uint16_t *samples_for_pitch(Pitch_Hz pitch_hz) {
    switch (pitch_hz) {
        case A5_HZ: return a5_samples;
        case C6_HZ: return c6_samples;
        case E6_HZ: return e6_samples;
        case G6_HZ: return g6_samples;
        case C7_HZ: return c7_samples;
        default: return NULL;
    }
}

static int play_note(const Note *note) {
	// reject unknown pitch just to be safe
    uint16_t *samples = samples_for_pitch(note->pitch_hz);
    if (samples == NULL) {
        return -1;
    }

    HAL_StatusTypeDef status = HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)samples, SAMPLE_CNT(note->pitch_hz), DAC_ALIGN_12B_R);

    if (status == HAL_OK) {
    	status = HAL_TIM_Base_Start(&htim2);
    }

    if (status == HAL_OK) {
	   osDelay(note->duration_ms);
   }

    // stop timer & dma reqs
    HAL_TIM_Base_Stop(&htim2);
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);

    return status == HAL_OK ? 0 : -1;
}

static int play_chime(const struct Note notes[], int notes_cnt) {
	for (int i = 0; i < notes_cnt; ++i) {
		if (play_note(&notes[i]) != 0) {
			return -1;
		}
	}

	return 0;
}

int play_tetris_chime(void) {
	return play_chime(tetris_chime, ARR_LEN(tetris_chime));
}

int play_game_over_chime(void) {
	return play_chime(game_over_chime, ARR_LEN(game_over_chime));
}

int audio_init(void) {
    init_pitch_samples(a5_samples, SAMPLE_CNT(A5_HZ));
    init_pitch_samples(c6_samples, SAMPLE_CNT(C6_HZ));
    init_pitch_samples(e6_samples, SAMPLE_CNT(E6_HZ));
    init_pitch_samples(g6_samples, SAMPLE_CNT(G6_HZ));
    init_pitch_samples(c7_samples, SAMPLE_CNT(C7_HZ));

    return 0;
}
