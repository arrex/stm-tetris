#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

typedef enum {
    AUDIO_LINE_CLEAR = 1U << 0,
    AUDIO_GAME_OVER = 1U << 1,
    AUDIO_START = 1U << 2,
} AudioEvent;

int audio_init(void);
// after init -> only the audio task may call these funcs
int audio_handle_event(uint32_t events, uint32_t now_ms);
int audio_update(uint32_t now_ms);
// ms until next note -> idle returns osWaitForever
uint32_t audio_wait_ms(uint32_t now_ms);

#endif
