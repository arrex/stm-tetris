#include <stdint.h>

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
