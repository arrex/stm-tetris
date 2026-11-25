#include "tetris.h"
#include "main.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// each row contains GRID_WIDTH cells followed by two newline bytes (\r\n)
#define ROW_LENGTH (GRID_WIDTH + 2)

#define PIECE_COUNT 7
#define PIECE_WIDTH 4
#define PIECE_HEIGHT 4

static const uint8_t tetrominoes[PIECE_COUNT][PIECE_WIDTH * PIECE_HEIGHT] = {
    {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // I
    {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0}, // O
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0}, // S
    {0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0}, // Z
    {0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0}, // T
    {0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0}, // L
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0}, // J
};

extern UART_HandleTypeDef huart1;

static uint8_t game_state[GRID_HEIGHT][GRID_WIDTH];
static uint8_t active_piece[GRID_HEIGHT][GRID_WIDTH];
static bool has_active_piece;
static bool game_over;

static void spawn_piece(void)
{
    const int piece = rand() % PIECE_COUNT;
    const int start_col = (GRID_WIDTH - PIECE_WIDTH) / 2;

    // Skip empty rows above the shape so it spawns at the top of the board.
    int first_occupied_row = 0;
    for (int cell = 0; cell < PIECE_WIDTH * PIECE_HEIGHT; ++cell) {
        if (tetrominoes[piece][cell]) {
            first_occupied_row = cell / PIECE_WIDTH;
            break;
        }
    }

    memset(active_piece, 0, sizeof(active_piece));
    has_active_piece = false;

    for (int row = first_occupied_row; row < PIECE_HEIGHT; ++row) {
        const int board_row = row - first_occupied_row;
        for (int col = 0; col < PIECE_WIDTH; ++col) {
            active_piece[board_row][start_col + col] = tetrominoes[piece][row * PIECE_WIDTH + col];

            if (active_piece[board_row][start_col + col] && game_state[board_row][start_col + col]) {
                // discard the blocked spawn and preserve the landed board
                memset(active_piece, 0, sizeof(active_piece));
                game_over = true;

                return;
            }
        }
    }

    has_active_piece = true;
}

int init_game_state(void)
{
    memset(game_state, 0, sizeof(game_state));
    game_over = false;
    spawn_piece();

    return 0;
}

static void clear_lines(void)
{
    int destination = GRID_HEIGHT - 1;

    // only landed blocks are in game state -> any full row can clear
    for (int row = GRID_HEIGHT - 1; row >= 0; --row) {
        int full = 1;
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (!game_state[row][col]) {
                full = 0;
                break;
            }
        }

        if (!full) {
            if (destination != row) {
                memcpy(game_state[destination], game_state[row], sizeof(game_state[row]));
            }
            --destination;
        }
    }

    // replace the cleared rows with empty rows at the top.
    while (destination >= 0) {
        memset(game_state[destination], 0, sizeof(game_state[destination]));
        --destination;
    }
}

static bool drop_blocks(void)
{
    // check the entire active piece before moving any of its cells
    for (int row = 0; row < GRID_HEIGHT; ++row) {
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (active_piece[row][col]
                && (row == GRID_HEIGHT - 1 || game_state[row + 1][col])) {
                return false;
            }
        }
    }

    // move active piece down one row
    for (int row = GRID_HEIGHT - 1; row > 0; --row) {
        memcpy(active_piece[row], active_piece[row - 1], sizeof(active_piece[row]));
    }

    memset(active_piece[0], 0, sizeof(active_piece[0]));
    return true;
}

static void lock_piece(void)
{
    for (int row = 0; row < GRID_HEIGHT; ++row) {
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (active_piece[row][col]) {
                game_state[row][col] = active_piece[row][col];
            }
        }
    }
    memset(active_piece, 0, sizeof(active_piece));
    has_active_piece = false;
}

int update_game_state(void)
{
    if (game_over) {
        // return special err code for game over
        return 99;
    }

    // there is an active piece, BUT it cannot move down further
    if (has_active_piece && !drop_blocks()) {
        lock_piece();
        clear_lines();
        spawn_piece();
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
            frame[frame_pos++] = (game_state[row][col] || active_piece[row][col]) ? '#' : '.';
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

    if (game_over) {
        static const uint8_t message[] = "Game over!\r\n";
        if (HAL_UART_Transmit(&huart1, message, sizeof(message) - 1U, 100U) != HAL_OK) {
            return -1;
        }
    }

    return 0;
}
