#ifndef TETRIS_H
#define TETRIS_H

#include <stdint.h>

#define GRID_WIDTH 10
#define GRID_HEIGHT 20

int init_game_state(void);
int update_game_state(void);
int draw_grid(void);

#endif
