/*
 * Snake Game
 *
 * 38x28 grid, 16px cells, arrow key controls.
 * Supports both fullscreen (snake_run) and windowed (init/key/tick/draw) modes.
 */
#include "snake.h"
#include "gpu.h"
#include "fb.h"
#include "input.h"
#include "timer.h"
#include "sched.h"

#define GRID_W  38
#define GRID_H  28
#define CELL    16
#define MAX_SNAKE (GRID_W * GRID_H)

#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

static u32 rng_state = 12345;
static u32 rng_next(void)
{
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state >> 16) & 0x7FFF;
}

static int snake_x[MAX_SNAKE];
static int snake_y[MAX_SNAKE];
static int snake_len;
static int dir;
static int food_x, food_y;
static int score;
static int game_over;
static u64 last_tick;
static int tick_interval;

static void place_food(void)
{
    for (int attempts = 0; attempts < 1000; attempts++) {
        int fx = (int)(rng_next() % GRID_W);
        int fy = (int)(rng_next() % GRID_H);
        int on_snake = 0;
        for (int i = 0; i < snake_len; i++) {
            if (snake_x[i] == fx && snake_y[i] == fy) {
                on_snake = 1;
                break;
            }
        }
        if (!on_snake) {
            food_x = fx;
            food_y = fy;
            return;
        }
    }
}

void snake_init(void)
{
    rng_state = (u32)timer_get_ticks();

    snake_len = 3;
    for (int i = 0; i < snake_len; i++) {
        snake_x[i] = GRID_W / 2 - i;
        snake_y[i] = GRID_H / 2;
    }
    dir = DIR_RIGHT;
    score = 0;
    game_over = 0;
    tick_interval = 12;
    last_tick = timer_get_ticks();
    place_food();
}

void snake_key(int key)
{
    if (!game_over) {
        if (key == KEY_UP    && dir != DIR_DOWN)  dir = DIR_UP;
        if (key == KEY_DOWN  && dir != DIR_UP)    dir = DIR_DOWN;
        if (key == KEY_LEFT  && dir != DIR_RIGHT) dir = DIR_LEFT;
        if (key == KEY_RIGHT && dir != DIR_LEFT)  dir = DIR_RIGHT;
    }
}

int snake_tick(void)
{
    if (game_over) return 0;

    u64 now = timer_get_ticks();
    if ((int)(now - last_tick) < tick_interval)
        return 0;
    last_tick = now;

    int nx = snake_x[0], ny = snake_y[0];
    switch (dir) {
    case DIR_UP:    ny--; break;
    case DIR_DOWN:  ny++; break;
    case DIR_LEFT:  nx--; break;
    case DIR_RIGHT: nx++; break;
    }

    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
        game_over = 1;
        return 1;
    }
    for (int i = 0; i < snake_len; i++) {
        if (snake_x[i] == nx && snake_y[i] == ny) {
            game_over = 1;
            return 1;
        }
    }

    int ate = (nx == food_x && ny == food_y);
    if (!ate) {
        for (int i = snake_len - 1; i > 0; i--) {
            snake_x[i] = snake_x[i - 1];
            snake_y[i] = snake_y[i - 1];
        }
    } else {
        if (snake_len < MAX_SNAKE) {
            for (int i = snake_len; i > 0; i--) {
                snake_x[i] = snake_x[i - 1];
                snake_y[i] = snake_y[i - 1];
            }
            snake_len++;
        }
        score += 10;
        place_food();
    }
    snake_x[0] = nx;
    snake_y[0] = ny;
    return 1;
}

void snake_draw(void)
{
    int tw = gpu_target_w();
    int th = gpu_target_h();
    int offset_x = (tw - GRID_W * CELL) / 2;
    int offset_y = 24;

    gpu_clear(COLOR_BLACK);

    gpu_draw_rect(offset_x - 1, offset_y - 1,
                  GRID_W * CELL + 2, GRID_H * CELL + 2, COLOR_GRAY);

    gpu_fill_rect(offset_x + food_x * CELL + 2, offset_y + food_y * CELL + 2,
                  CELL - 4, CELL - 4, COLOR_RED);

    for (int i = 0; i < snake_len; i++) {
        u32 color = (i == 0) ? COLOR_GREEN : COLOR_DKGREEN;
        gpu_fill_rect(offset_x + snake_x[i] * CELL + 1,
                      offset_y + snake_y[i] * CELL + 1,
                      CELL - 2, CELL - 2, color);
    }

    gpu_draw_string(8, 4, "SNAKE  Score:", COLOR_WHITE, COLOR_BLACK);
    gpu_draw_int(8 + 14 * FONT_WIDTH, 4, score, COLOR_YELLOW, COLOR_BLACK);

    if (game_over) {
        int gx = (tw - 9 * FONT_WIDTH) / 2;
        int gy = th / 2 - 8;
        gpu_fill_rect(gx - 16, gy - 8, 9 * FONT_WIDTH + 32, FONT_HEIGHT + 16, COLOR_DKRED);
        gpu_draw_string(gx, gy, "GAME OVER", COLOR_WHITE, COLOR_DKRED);
    }
}

void snake_run(void)
{
    snake_init();
    u64 last_frame = timer_get_ticks();

    for (;;) {
        int key = input_poll();
        while (key != KEY_NONE) {
            if (key == KEY_ESC) return;
            snake_key(key);
            key = input_poll();
        }

        snake_tick();

        u64 now = timer_get_ticks();
        if ((int)(now - last_frame) >= 3) {
            last_frame = now;
            snake_draw();
            gpu_flip();
        }

        sched_yield();
    }
}
