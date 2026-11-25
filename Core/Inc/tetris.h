#ifndef TETRIS_H
#define TETRIS_H

#include <stdint.h>
#include <stdbool.h>

#define GRID_WIDTH 10
#define GRID_HEIGHT 20

typedef struct {
    uint8_t board[GRID_HEIGHT][GRID_WIDTH];
    uint8_t active_piece[GRID_HEIGHT][GRID_WIDTH];
    uint32_t lines_cleared;
    bool has_active_piece;
    bool game_over;
} GameState;

/* The caller synchronizes access when sharing state between tasks. */
int init_game_state(GameState *state);
int update_game_state(GameState *state);
int rotate_piece(GameState *state);
/* Render an unchanged snapshot without holding the game-state mutex. */
int draw_grid(const GameState *state);

#endif
