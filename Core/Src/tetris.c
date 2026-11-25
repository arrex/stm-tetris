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

static void spawn_piece(GameState *state)
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

    memset(state->active_piece, 0, sizeof(state->active_piece));
    state->has_active_piece = false;

    for (int row = first_occupied_row; row < PIECE_HEIGHT; ++row) {
        const int board_row = row - first_occupied_row;
        for (int col = 0; col < PIECE_WIDTH; ++col) {
            state->active_piece[board_row][start_col + col] = tetrominoes[piece][row * PIECE_WIDTH + col];

            if (state->active_piece[board_row][start_col + col] && state->board[board_row][start_col + col]) {
                // discard the blocked spawn and preserve the landed board
                memset(state->active_piece, 0, sizeof(state->active_piece));
                state->game_over = true;

                return;
            }
        }
    }

    state->has_active_piece = true;
}

int init_game_state(GameState *state)
{
    if (state == NULL) {
        return -1;
    }

    memset(state, 0, sizeof(*state));
    spawn_piece(state);

    return 0;
}

static int clear_lines(GameState *state)
{
    int destination = GRID_HEIGHT - 1;
    int cleared = 0;

    // only landed blocks are in game state -> any full row can clear
    for (int row = GRID_HEIGHT - 1; row >= 0; --row) {
        int full = 1;
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (!state->board[row][col]) {
                full = 0;
                break;
            }
        }

        if (!full) {
            if (destination != row) {
                memcpy(state->board[destination], state->board[row], sizeof(state->board[row]));
            }
            --destination;
        } else {
            ++cleared;
        }
    }

    // replace the cleared rows with empty rows at the top.
    while (destination >= 0) {
        memset(state->board[destination], 0, sizeof(state->board[destination]));
        --destination;
    }
    return cleared;
}

static bool drop_blocks(GameState *state)
{
    // check the entire active piece before moving any of its cells
    for (int row = 0; row < GRID_HEIGHT; ++row) {
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (state->active_piece[row][col]
                && (row == GRID_HEIGHT - 1 || state->board[row + 1][col])) {
                return false;
            }
        }
    }

    // move active piece down one row
    for (int row = GRID_HEIGHT - 1; row > 0; --row) {
        memcpy(state->active_piece[row], state->active_piece[row - 1], sizeof(state->active_piece[row]));
    }

    memset(state->active_piece[0], 0, sizeof(state->active_piece[0]));
    return true;
}

static void lock_piece(GameState *state)
{
    for (int row = 0; row < GRID_HEIGHT; ++row) {
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (state->active_piece[row][col]) {
                state->board[row][col] = state->active_piece[row][col];
            }
        }
    }
    memset(state->active_piece, 0, sizeof(state->active_piece));
    state->has_active_piece = false;
}

int rotate_piece(GameState *state)
{
    if (state == NULL) {
        return -1;
    }

    if (state->game_over || !state->has_active_piece) {
        return 0;
    }

    // find the occupied rectangle -> keep top-left corner fixed
    int top = GRID_HEIGHT, bottom = -1;
    int left = GRID_WIDTH, right = -1;
    for (int row = 0; row < GRID_HEIGHT; ++row) {
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (state->active_piece[row][col]) {
                if (row < top) top = row;
                if (row > bottom) bottom = row;
                if (col < left) left = col;
                if (col > right) right = col;
            }
        }
    }

    if (bottom < 0) {
        return 0;
    }

    const int height = bottom - top + 1;
    const int width = right - left + 1;

    // rotation swaps the rectangle's width and height
    if (top + width > GRID_HEIGHT || left + height > GRID_WIDTH) {
        return 0;
    }

    uint8_t rotated[PIECE_HEIGHT][PIECE_WIDTH] = {0};
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (state->active_piece[top + row][left + col]) {
                const int new_row = col;
                const int new_col = height - 1 - row;
                if (state->board[top + new_row][left + new_col]) {
                    return 0;
                }
                rotated[new_row][new_col] = state->active_piece[top + row][left + col];
            }
        }
    }

    // commit only after every rotated cell passes the collision check
    memset(state->active_piece, 0, sizeof(state->active_piece));
    for (int row = 0; row < width; ++row) {
        for (int col = 0; col < height; ++col) {
            state->active_piece[top + row][left + col] = rotated[row][col];
        }
    }
    return 0;
}

int move_piece(GameState *state, int direction)
{
    if (state == NULL || (direction != -1 && direction != 1)) {
        return -1;
    }
    if (state->game_over || !state->has_active_piece) {
        return 0;
    }

    // Check every destination before moving any part of the piece.
    for (int row = 0; row < GRID_HEIGHT; ++row) {
        for (int col = 0; col < GRID_WIDTH; ++col) {
            if (state->active_piece[row][col]) {
                const int next_col = col + direction;
                if (next_col < 0 || next_col >= GRID_WIDTH || state->board[row][next_col]) {
                    return 0;
                }
            }
        }
    }

    for (int row = 0; row < GRID_HEIGHT; ++row) {
        if (direction == -1) {
            memmove(state->active_piece[row], state->active_piece[row] + 1, GRID_WIDTH - 1);
            state->active_piece[row][GRID_WIDTH - 1] = 0;
        } else {
            memmove(state->active_piece[row] + 1, state->active_piece[row], GRID_WIDTH - 1);
            state->active_piece[row][0] = 0;
        }
    }
    return 0;
}

int update_game_state(GameState *state)
{
    if (state == NULL) {
        return -1;
    }

    if (state->game_over) {
        // return special err code for game over
        return 99;
    }

    // there is an active piece, BUT it cannot move down further
    if (state->has_active_piece && !drop_blocks(state)) {
        lock_piece(state);
        state->lines_cleared += clear_lines(state);
        spawn_piece(state);
    }

    return 0;
}

int draw_grid(const GameState *state)
{
    if (state == NULL) {
        return -1;
    }

    static const uint8_t clear_screen[] = "\033[2J\033[H";
    static uint8_t frame[GRID_HEIGHT * ROW_LENGTH];
    uint16_t frame_pos = 0;

    for (int row = 0; row < GRID_HEIGHT; ++row) {
    	// populate each row
        for (int col = 0; col < GRID_WIDTH; ++col) {
            frame[frame_pos++] = (state->board[row][col] || state->active_piece[row][col]) ? '#' : '.';
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

    if (state->game_over) {
        static const uint8_t message[] = "Game over!\r\n";
        if (HAL_UART_Transmit(&huart1, message, sizeof(message) - 1U, 100U) != HAL_OK) {
            return -1;
        }
    }

    return 0;
}
