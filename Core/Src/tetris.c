#include "tetris.h"
#include "main.h"
#include <string.h>

// each row contains GRID_WIDTH cells followed by two newline bytes (\r\n)
#define ROW_LENGTH (GRID_WIDTH + 2)

extern UART_HandleTypeDef huart1;

static uint8_t game_state[GRID_HEIGHT][GRID_WIDTH];

int init_game_state(void)
{
    memset(game_state, 0, sizeof(game_state));

    const int bar_length = 4;
    const int bar_start = (GRID_WIDTH - bar_length) / 2;
    for (int col = bar_start; col < bar_start + bar_length; ++col) {
        game_state[0][col] = 1;
    }

    return 0;
}

int draw_grid(void)
{
    static const uint8_t clear_screen[] = "\033[2J\033[H";

    static uint8_t frame[GRID_HEIGHT * ROW_LENGTH];
    uint16_t frame_pos = 0;

    for (int row = 0; row < GRID_HEIGHT; ++row) {
    	// populate each row
        for (int col = 0; col < GRID_WIDTH; ++col) {
            frame[frame_pos++] = game_state[row][col] ? '#' : '.';
        }

        // add newline chars
        frame[frame_pos++] = '\r';
        frame[frame_pos++] = '\n';
    }

    // clear terminal, then send the grid
    if (HAL_UART_Transmit(&huart1, clear_screen, sizeof(clear_screen) - 1U, 100U) != HAL_OK
        || HAL_UART_Transmit(&huart1, frame, sizeof(frame), 100U) != HAL_OK) {
        return -1;
    }

    return 0;
}
