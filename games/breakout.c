/*
 * Breakout Game
 *
 * Paddle, bouncing ball, 10x5 brick grid.
 * 8.8 fixed-point ball physics.
 * Supports both fullscreen and windowed modes.
 */
#include "breakout.h"
#include "gpu.h"
#include "fb.h"
#include "input.h"
#include "timer.h"
#include "sched.h"

#define FX_SHIFT 8
#define FX_ONE   (1 << FX_SHIFT)
#define FX_TO_INT(a) ((a) >> FX_SHIFT)
#define INT_TO_FX(a) ((a) << FX_SHIFT)

#define BRICK_COLS   10
#define BRICK_ROWS   5
#define BRICK_W      58
#define BRICK_H      16
#define BRICK_GAP    4
#define BRICK_OFFSET_Y 40

#define PADDLE_H    12
#define PADDLE_SPEED 8
#define BALL_SIZE   8

static u8 bricks[BRICK_ROWS][BRICK_COLS];
static int paddle_x, paddle_w;
static int ball_x, ball_y;
static int ball_dx, ball_dy;
static int score, lives;
static int game_over, game_won;
static int bricks_left;
static u64 last_tick;

/* Dimensions used for layout (set from render target) */
static int scr_w, scr_h;
static int paddle_y;
static int brick_offset_x;

static const u32 brick_colors[BRICK_ROWS] = {
    COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN
};

static void reset_ball(void)
{
    ball_x = INT_TO_FX(scr_w / 2);
    ball_y = INT_TO_FX(paddle_y - 20);
    ball_dx = FX_ONE * 2;
    ball_dy = -(FX_ONE * 2);
}

static void init_bricks(void)
{
    bricks_left = 0;
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            bricks[r][c] = 1;
            bricks_left++;
        }
    }
}

void breakout_init(void)
{
    scr_w = gpu_target_w();
    scr_h = gpu_target_h();
    paddle_w = 80;
    paddle_y = scr_h - 40;
    brick_offset_x = (scr_w - BRICK_COLS * (BRICK_W + BRICK_GAP) + BRICK_GAP) / 2;

    paddle_x = (scr_w - paddle_w) / 2;
    score = 0;
    lives = 3;
    game_over = 0;
    game_won = 0;
    init_bricks();
    reset_ball();
    last_tick = timer_get_ticks();
}

void breakout_key(int key)
{
    if (game_over || game_won) return;
    if ((key == KEY_LEFT || key == 'a' || key == 'A') && paddle_x > 0) {
        paddle_x -= PADDLE_SPEED;
        if (paddle_x < 0) paddle_x = 0;
    }
    if ((key == KEY_RIGHT || key == 'd' || key == 'D') &&
        paddle_x < scr_w - paddle_w) {
        paddle_x += PADDLE_SPEED;
        if (paddle_x > scr_w - paddle_w)
            paddle_x = scr_w - paddle_w;
    }
}

int breakout_tick(void)
{
    if (game_over || game_won) return 0;

    u64 now = timer_get_ticks();
    if ((int)(now - last_tick) < 3) return 0;
    last_tick = now;

    ball_x += ball_dx;
    ball_y += ball_dy;

    int bx = FX_TO_INT(ball_x);
    int by = FX_TO_INT(ball_y);

    if (bx <= BALL_SIZE / 2) {
        ball_x = INT_TO_FX(BALL_SIZE / 2);
        ball_dx = -ball_dx;
    }
    if (bx >= scr_w - BALL_SIZE / 2) {
        ball_x = INT_TO_FX(scr_w - BALL_SIZE / 2);
        ball_dx = -ball_dx;
    }
    if (by <= BALL_SIZE / 2) {
        ball_y = INT_TO_FX(BALL_SIZE / 2);
        ball_dy = -ball_dy;
    }

    if (by >= scr_h) {
        lives--;
        if (lives <= 0) {
            game_over = 1;
        } else {
            reset_ball();
        }
        return 1;
    }

    if (ball_dy > 0 && by >= paddle_y - BALL_SIZE / 2 && by < paddle_y + PADDLE_H &&
        bx >= paddle_x && bx <= paddle_x + paddle_w) {
        ball_dy = -ball_dy;
        int hit_pos = bx - paddle_x - paddle_w / 2;
        ball_dx = (hit_pos * FX_ONE * 3) / (paddle_w / 2);
        if (ball_dx > FX_ONE * 3) ball_dx = FX_ONE * 3;
        if (ball_dx < -(FX_ONE * 3)) ball_dx = -(FX_ONE * 3);
        ball_y = INT_TO_FX(paddle_y - BALL_SIZE / 2);
    }

    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c]) continue;
            int brk_x = brick_offset_x + c * (BRICK_W + BRICK_GAP);
            int brk_y = BRICK_OFFSET_Y + r * (BRICK_H + BRICK_GAP);

            if (bx + BALL_SIZE / 2 >= brk_x && bx - BALL_SIZE / 2 <= brk_x + BRICK_W &&
                by + BALL_SIZE / 2 >= brk_y && by - BALL_SIZE / 2 <= brk_y + BRICK_H) {
                bricks[r][c] = 0;
                bricks_left--;
                score += (BRICK_ROWS - r) * 10;
                ball_dy = -ball_dy;
                if (bricks_left <= 0)
                    game_won = 1;
                return 1;
            }
        }
    }

    return 1;
}

void breakout_draw(void)
{
    int tw = gpu_target_w();
    int th = gpu_target_h();

    gpu_clear(COLOR_BLACK);

    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c]) continue;
            int bx = brick_offset_x + c * (BRICK_W + BRICK_GAP);
            int by = BRICK_OFFSET_Y + r * (BRICK_H + BRICK_GAP);
            gpu_fill_rect(bx, by, BRICK_W, BRICK_H, brick_colors[r]);
        }
    }

    gpu_fill_rect(paddle_x, paddle_y, paddle_w, PADDLE_H, COLOR_WHITE);

    int bx = FX_TO_INT(ball_x) - BALL_SIZE / 2;
    int by = FX_TO_INT(ball_y) - BALL_SIZE / 2;
    gpu_fill_rect(bx, by, BALL_SIZE, BALL_SIZE, COLOR_WHITE);

    gpu_draw_string(8, 4, "BREAKOUT  Score:", COLOR_WHITE, COLOR_BLACK);
    gpu_draw_int(8 + 17 * FONT_WIDTH, 4, score, COLOR_YELLOW, COLOR_BLACK);

    gpu_draw_string(8, 20, "Lives:", COLOR_WHITE, COLOR_BLACK);
    for (int i = 0; i < lives; i++)
        gpu_fill_rect(8 + 7 * FONT_WIDTH + i * 16, 22, 10, 10, COLOR_RED);

    if (game_over) {
        int gx = (tw - 9 * FONT_WIDTH) / 2;
        int gy = th / 2;
        gpu_fill_rect(gx - 16, gy - 8, 9 * FONT_WIDTH + 32, FONT_HEIGHT + 16, COLOR_DKRED);
        gpu_draw_string(gx, gy, "GAME OVER", COLOR_WHITE, COLOR_DKRED);
    }
    if (game_won) {
        int gx = (tw - 8 * FONT_WIDTH) / 2;
        int gy = th / 2;
        gpu_fill_rect(gx - 16, gy - 8, 8 * FONT_WIDTH + 32, FONT_HEIGHT + 16, COLOR_DKGREEN);
        gpu_draw_string(gx, gy, "YOU WIN!", COLOR_WHITE, COLOR_DKGREEN);
    }
}

void breakout_run(void)
{
    /* In fullscreen, set target dims from FB */
    scr_w = FB_WIDTH;
    scr_h = FB_HEIGHT;
    paddle_w = 80;
    paddle_y = scr_h - 40;
    brick_offset_x = (scr_w - BRICK_COLS * (BRICK_W + BRICK_GAP) + BRICK_GAP) / 2;

    paddle_x = (scr_w - paddle_w) / 2;
    score = 0;
    lives = 3;
    game_over = 0;
    game_won = 0;
    init_bricks();
    reset_ball();

    u64 last_phys = timer_get_ticks();
    u64 last_frame = timer_get_ticks();

    for (;;) {
        int key = input_poll();
        while (key != KEY_NONE) {
            if (key == KEY_ESC) return;
            breakout_key(key);
            key = input_poll();
        }

        u64 now = timer_get_ticks();
        if ((int)(now - last_phys) >= 3) {
            last_phys = now;
            last_tick = now - 3; /* force tick */
            breakout_tick();
        }

        now = timer_get_ticks();
        if ((int)(now - last_frame) >= 3) {
            last_frame = now;
            breakout_draw();
            gpu_flip();
        }

        sched_yield();
    }
}
