/*
 * Tetris Game
 *
 * 10x20 board, 7 tetrominoes, 4 rotations each.
 * Supports both fullscreen and windowed modes.
 */
#include "tetris.h"
#include "gpu.h"
#include "fb.h"
#include "input.h"
#include "timer.h"
#include "sched.h"

#define BOARD_W   10
#define BOARD_H   20
#define CELL_SIZE 20

static const u16 pieces[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    {0x6600, 0x6600, 0x6600, 0x6600},
    {0x0E40, 0x4C40, 0x4E00, 0x4640},
    {0x06C0, 0x8C40, 0x6C00, 0x4620},
    {0x0C60, 0x4C80, 0xC600, 0x2640},
    {0x0E80, 0xC440, 0x2E00, 0x44C0},
    {0x0E20, 0x44C0, 0x8E00, 0xC440},
};

static const u32 piece_colors[7] = {
    COLOR_CYAN, COLOR_YELLOW, COLOR_MAGENTA,
    COLOR_GREEN, COLOR_RED, COLOR_BLUE, COLOR_ORANGE
};

static u32 rng_state;
static int rng_piece(void)
{
    rng_state = rng_state * 1103515245 + 12345;
    return (int)((rng_state >> 16) % 7);
}

static u8 board[BOARD_H][BOARD_W];
static int cur_piece, cur_rot, cur_x, cur_y;
static int next_piece;
static int score, level, lines;
static int game_over;
static u64 last_drop;
static int drop_interval;

static int piece_bit(int piece, int rot, int r, int c)
{
    u16 mask = pieces[piece][rot];
    return (mask >> (15 - (r * 4 + c))) & 1;
}

static int fits(int piece, int rot, int px, int py)
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!piece_bit(piece, rot, r, c)) continue;
            int bx = px + c, by = py + r;
            if (bx < 0 || bx >= BOARD_W || by >= BOARD_H)
                return 0;
            if (by >= 0 && board[by][bx])
                return 0;
        }
    }
    return 1;
}

static void lock_piece(void)
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!piece_bit(cur_piece, cur_rot, r, c)) continue;
            int bx = cur_x + c, by = cur_y + r;
            if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W)
                board[by][bx] = (u8)(cur_piece + 1);
        }
    }
}

static int clear_lines(void)
{
    int cleared = 0;
    for (int r = BOARD_H - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < BOARD_W; c++) {
            if (!board[r][c]) { full = 0; break; }
        }
        if (full) {
            cleared++;
            for (int rr = r; rr > 0; rr--) {
                for (int c = 0; c < BOARD_W; c++)
                    board[rr][c] = board[rr - 1][c];
            }
            for (int c = 0; c < BOARD_W; c++)
                board[0][c] = 0;
            r++;
        }
    }
    return cleared;
}

static void spawn_piece(void)
{
    cur_piece = next_piece;
    next_piece = rng_piece();
    cur_rot = 0;
    cur_x = BOARD_W / 2 - 2;
    cur_y = -1;
    if (!fits(cur_piece, cur_rot, cur_x, cur_y))
        game_over = 1;
}

void tetris_init(void)
{
    rng_state = (u32)timer_get_ticks();

    for (int r = 0; r < BOARD_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            board[r][c] = 0;

    score = 0;
    level = 1;
    lines = 0;
    game_over = 0;
    drop_interval = 50;
    next_piece = rng_piece();
    spawn_piece();
    last_drop = timer_get_ticks();
}

void tetris_key(int key)
{
    if (game_over) return;
    if (key == KEY_LEFT && fits(cur_piece, cur_rot, cur_x - 1, cur_y))
        cur_x--;
    else if (key == KEY_RIGHT && fits(cur_piece, cur_rot, cur_x + 1, cur_y))
        cur_x++;
    else if (key == KEY_DOWN && fits(cur_piece, cur_rot, cur_x, cur_y + 1))
        cur_y++;
    else if (key == KEY_UP) {
        int new_rot = (cur_rot + 1) & 3;
        if (fits(cur_piece, new_rot, cur_x, cur_y))
            cur_rot = new_rot;
    }
    else if (key == KEY_SPACE) {
        while (fits(cur_piece, cur_rot, cur_x, cur_y + 1))
            cur_y++;
    }
}

int tetris_tick(void)
{
    if (game_over) return 0;

    u64 now = timer_get_ticks();
    if ((int)(now - last_drop) < drop_interval) return 0;
    last_drop = now;

    if (fits(cur_piece, cur_rot, cur_x, cur_y + 1)) {
        cur_y++;
    } else {
        lock_piece();
        int cl = clear_lines();
        if (cl > 0) {
            static const int scores[] = {0, 100, 300, 500, 800};
            score += scores[cl] * level;
            lines += cl;
            level = 1 + lines / 10;
            drop_interval = 50 - (level - 1) * 4;
            if (drop_interval < 8) drop_interval = 8;
        }
        spawn_piece();
    }
    return 1;
}

void tetris_draw(void)
{
    int tw = gpu_target_w();
    int th = gpu_target_h();
    int board_x = (tw - BOARD_W * CELL_SIZE) / 2;
    int board_y = 20;

    gpu_clear(COLOR_BLACK);

    gpu_draw_rect(board_x - 1, board_y - 1,
                  BOARD_W * CELL_SIZE + 2, BOARD_H * CELL_SIZE + 2, COLOR_GRAY);

    for (int r = 0; r < BOARD_H; r++) {
        for (int c = 0; c < BOARD_W; c++) {
            if (board[r][c]) {
                int px = board_x + c * CELL_SIZE;
                int py = board_y + r * CELL_SIZE;
                u32 col = piece_colors[board[r][c] - 1];
                gpu_fill_rect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, col);
            }
        }
    }

    if (!game_over) {
        u32 col = piece_colors[cur_piece];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (!piece_bit(cur_piece, cur_rot, r, c)) continue;
                int bx = cur_x + c, by = cur_y + r;
                if (by >= 0) {
                    int px = board_x + bx * CELL_SIZE;
                    int py = board_y + by * CELL_SIZE;
                    gpu_fill_rect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, col);
                }
            }
        }
    }

    int nx_x = board_x + BOARD_W * CELL_SIZE + 20;
    int nx_y = board_y + 20;
    gpu_draw_string(nx_x, nx_y - 20, "Next:", COLOR_WHITE, COLOR_BLACK);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (piece_bit(next_piece, 0, r, c)) {
                gpu_fill_rect(nx_x + c * 12, nx_y + r * 12, 10, 10,
                              piece_colors[next_piece]);
            }
        }
    }

    int info_x = board_x + BOARD_W * CELL_SIZE + 20;
    gpu_draw_string(info_x, board_y + 120, "Score:", COLOR_WHITE, COLOR_BLACK);
    gpu_draw_int(info_x, board_y + 140, score, COLOR_YELLOW, COLOR_BLACK);
    gpu_draw_string(info_x, board_y + 170, "Level:", COLOR_WHITE, COLOR_BLACK);
    gpu_draw_int(info_x, board_y + 190, level, COLOR_YELLOW, COLOR_BLACK);
    gpu_draw_string(info_x, board_y + 220, "Lines:", COLOR_WHITE, COLOR_BLACK);
    gpu_draw_int(info_x, board_y + 240, lines, COLOR_YELLOW, COLOR_BLACK);

    if (game_over) {
        int gx = (tw - 9 * FONT_WIDTH) / 2;
        int gy = th / 2 - 8;
        gpu_fill_rect(gx - 16, gy - 8, 9 * FONT_WIDTH + 32, FONT_HEIGHT + 16, COLOR_DKRED);
        gpu_draw_string(gx, gy, "GAME OVER", COLOR_WHITE, COLOR_DKRED);
    }
}

void tetris_run(void)
{
    tetris_init();
    u64 last_frame = timer_get_ticks();

    for (;;) {
        int key = input_poll();
        while (key != KEY_NONE) {
            if (key == KEY_ESC) return;
            tetris_key(key);
            key = input_poll();
        }

        tetris_tick();

        u64 now = timer_get_ticks();
        if ((int)(now - last_frame) >= 3) {
            last_frame = now;
            tetris_draw();
            gpu_flip();
        }

        sched_yield();
    }
}
