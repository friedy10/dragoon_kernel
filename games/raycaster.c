/*
 * Wolfenstein-style 3D Raycaster
 *
 * 16x16 map, DDA raycasting, 16.16 fixed-point math.
 * WASD movement, arrow key rotation.
 * All integer math (no floating-point).
 * Supports both fullscreen and windowed modes.
 */
#include "raycaster.h"
#include "gpu.h"
#include "fb.h"
#include "input.h"
#include "timer.h"
#include "sched.h"

/* Fixed-point 16.16 */
#define FP_SHIFT 16
#define FP_ONE   (1 << FP_SHIFT)
#define FP_HALF  (FP_ONE >> 1)
#define FP_MUL(a, b) ((int)(((s64)(a) * (s64)(b)) >> FP_SHIFT))
#define FP_DIV(a, b) ((int)(((s64)(a) << FP_SHIFT) / (b)))
#define FP_TO_INT(a) ((a) >> FP_SHIFT)
#define INT_TO_FP(a) ((a) << FP_SHIFT)

#define ANGLE_360 256
#define ANGLE_90  64
#define ANGLE_180 128
#define ANGLE_270 192

static const int sin_table[256] = {
        0,   1608,   3216,   4821,   6424,   8022,   9616,  11204,
    12785,  14359,  15924,  17479,  19024,  20557,  22078,  23586,
    25080,  26558,  28020,  29466,  30893,  32303,  33692,  35062,
    36410,  37736,  39040,  40320,  41576,  42806,  44011,  45190,
    46341,  47464,  48559,  49624,  50660,  51665,  52639,  53581,
    54491,  55368,  56212,  57022,  57798,  58538,  59244,  59914,
    60547,  61145,  61705,  62228,  62714,  63162,  63572,  63944,
    64277,  64571,  64827,  65043,  65220,  65358,  65457,  65516,
    65536,  65516,  65457,  65358,  65220,  65043,  64827,  64571,
    64277,  63944,  63572,  63162,  62714,  62228,  61705,  61145,
    60547,  59914,  59244,  58538,  57798,  57022,  56212,  55368,
    54491,  53581,  52639,  51665,  50660,  49624,  48559,  47464,
    46341,  45190,  44011,  42806,  41576,  40320,  39040,  37736,
    36410,  35062,  33692,  32303,  30893,  29466,  28020,  26558,
    25080,  23586,  22078,  20557,  19024,  17479,  15924,  14359,
    12785,  11204,   9616,   8022,   6424,   4821,   3216,   1608,
        0,  -1608,  -3216,  -4821,  -6424,  -8022,  -9616, -11204,
   -12785, -14359, -15924, -17479, -19024, -20557, -22078, -23586,
   -25080, -26558, -28020, -29466, -30893, -32303, -33692, -35062,
   -36410, -37736, -39040, -40320, -41576, -42806, -44011, -45190,
   -46341, -47464, -48559, -49624, -50660, -51665, -52639, -53581,
   -54491, -55368, -56212, -57022, -57798, -58538, -59244, -59914,
   -60547, -61145, -61705, -62228, -62714, -63162, -63572, -63944,
   -64277, -64571, -64827, -65043, -65220, -65358, -65457, -65516,
   -65536, -65516, -65457, -65358, -65220, -65043, -64827, -64571,
   -64277, -63944, -63572, -63162, -62714, -62228, -61705, -61145,
   -60547, -59914, -59244, -58538, -57798, -57022, -56212, -55368,
   -54491, -53581, -52639, -51665, -50660, -49624, -48559, -47464,
   -46341, -45190, -44011, -42806, -41576, -40320, -39040, -37736,
   -36410, -35062, -33692, -32303, -30893, -29466, -28020, -26558,
   -25080, -23586, -22078, -20557, -19024, -17479, -15924, -14359,
   -12785, -11204,  -9616,  -8022,  -6424,  -4821,  -3216,  -1608,
};

static int fp_sin(int angle)  { return sin_table[angle & 0xFF]; }
static int fp_cos(int angle)  { return sin_table[(angle + ANGLE_90) & 0xFF]; }
static int fp_abs(int x)      { return x < 0 ? -x : x; }

#define MAP_W 16
#define MAP_H 16

static const u8 world_map[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,3,3,3,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,3,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,4,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,0,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,0,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,4,4,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,3,0,0,0,1},
    {1,0,0,2,2,0,0,0,0,0,3,3,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

static const u32 wall_colors[5] = {
    COLOR_GRAY, COLOR_WHITE, COLOR_RED, COLOR_BLUE, COLOR_GREEN
};

static int player_x, player_y;
static int player_angle;
static int move_speed;
static int rot_speed;

#define FOV 42

static void cast_rays(void)
{
    int half_fov = FOV / 2;
    /* Use gpu_backbuf() which returns cur_buf when render target is set */
    u32 *buf = gpu_backbuf();
    int scr_w = gpu_target_w();
    int scr_h = gpu_target_h();

    for (int col = 0; col < scr_w; col++) {
        int ray_angle = player_angle - half_fov + (col * FOV) / scr_w;
        ray_angle &= 0xFF;

        int ray_cos = fp_cos(ray_angle);
        int ray_sin = fp_sin(ray_angle);

        int map_x = FP_TO_INT(player_x);
        int map_y = FP_TO_INT(player_y);

        int delta_dist_x = ray_cos != 0 ? fp_abs(FP_DIV(FP_ONE, ray_cos)) : 0x7FFFFFFF;
        int delta_dist_y = ray_sin != 0 ? fp_abs(FP_DIV(FP_ONE, ray_sin)) : 0x7FFFFFFF;

        int step_x, step_y;
        int side_dist_x, side_dist_y;

        if (ray_cos < 0) {
            step_x = -1;
            side_dist_x = FP_MUL(player_x - INT_TO_FP(map_x), delta_dist_x);
        } else {
            step_x = 1;
            side_dist_x = FP_MUL(INT_TO_FP(map_x + 1) - player_x, delta_dist_x);
        }
        if (ray_sin < 0) {
            step_y = -1;
            side_dist_y = FP_MUL(player_y - INT_TO_FP(map_y), delta_dist_y);
        } else {
            step_y = 1;
            side_dist_y = FP_MUL(INT_TO_FP(map_y + 1) - player_y, delta_dist_y);
        }

        int hit = 0, side = 0;
        int wall_type = 1;
        for (int step = 0; step < 64; step++) {
            if (side_dist_x < side_dist_y) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }
            if (map_x >= 0 && map_x < MAP_W && map_y >= 0 && map_y < MAP_H) {
                if (world_map[map_y][map_x]) {
                    hit = 1;
                    wall_type = world_map[map_y][map_x];
                    break;
                }
            } else {
                hit = 1;
                break;
            }
        }

        if (!hit) continue;

        int perp_dist;
        if (side == 0)
            perp_dist = side_dist_x - delta_dist_x;
        else
            perp_dist = side_dist_y - delta_dist_y;

        if (perp_dist < 256) perp_dist = 256;

        int angle_diff = ray_angle - player_angle;
        angle_diff &= 0xFF;
        int cos_diff = fp_cos(angle_diff);
        if (cos_diff == 0) cos_diff = 1;
        perp_dist = FP_MUL(perp_dist, cos_diff);
        if (perp_dist < 256) perp_dist = 256;

        int line_height = FP_DIV(INT_TO_FP(scr_h), perp_dist);
        line_height = FP_TO_INT(line_height);
        if (line_height > scr_h) line_height = scr_h;

        int draw_start = (scr_h - line_height) / 2;
        int draw_end = draw_start + line_height;

        u32 color = wall_colors[wall_type < 5 ? wall_type : 0];
        if (side)
            color = ((color >> 1) & 0x7F7F7F);

        /* Draw column directly to buffer */
        for (int y = 0; y < draw_start; y++)
            buf[y * scr_w + col] = COLOR_DKGRAY;
        for (int y = draw_start; y < draw_end && y < scr_h; y++) {
            if (y >= 0)
                buf[y * scr_w + col] = color;
        }
        for (int y = draw_end; y < scr_h; y++)
            buf[y * scr_w + col] = COLOR_DKGREEN;
    }
}

static void draw_minimap(void)
{
    int scale = 4;
    int ox = 4, oy = 4;

    for (int my = 0; my < MAP_H; my++) {
        for (int mx = 0; mx < MAP_W; mx++) {
            u32 color = world_map[my][mx] ?
                        wall_colors[world_map[my][mx]] : COLOR_DKGRAY;
            gpu_fill_rect(ox + mx * scale, oy + my * scale, scale, scale, color);
        }
    }

    int px = ox + FP_TO_INT(player_x) * scale;
    int py = oy + FP_TO_INT(player_y) * scale;
    gpu_fill_rect(px, py, 2, 2, COLOR_YELLOW);
}

void raycaster_init(void)
{
    player_x = INT_TO_FP(2) + FP_HALF;
    player_y = INT_TO_FP(2) + FP_HALF;
    player_angle = 0;
    move_speed = FP_ONE / 8;
    rot_speed = 3;
}

void raycaster_key(int key)
{
    if (key == KEY_LEFT || key == 'a' || key == 'A')
        player_angle = (player_angle - rot_speed) & 0xFF;
    if (key == KEY_RIGHT || key == 'd' || key == 'D')
        player_angle = (player_angle + rot_speed) & 0xFF;

    int dx = FP_MUL(fp_cos(player_angle), move_speed);
    int dy = FP_MUL(fp_sin(player_angle), move_speed);

    if (key == KEY_UP || key == 'w' || key == 'W') {
        int nx = player_x + dx;
        int ny = player_y + dy;
        int mx = FP_TO_INT(nx), my = FP_TO_INT(ny);
        if (mx >= 0 && mx < MAP_W && my >= 0 && my < MAP_H &&
            !world_map[my][mx]) {
            player_x = nx;
            player_y = ny;
        }
    }
    if (key == KEY_DOWN || key == 's' || key == 'S') {
        int nx = player_x - dx;
        int ny = player_y - dy;
        int mx = FP_TO_INT(nx), my = FP_TO_INT(ny);
        if (mx >= 0 && mx < MAP_W && my >= 0 && my < MAP_H &&
            !world_map[my][mx]) {
            player_x = nx;
            player_y = ny;
        }
    }
}

int raycaster_tick(void)
{
    /* Raycaster is driven by key input, no autonomous tick */
    return 0;
}

void raycaster_draw(void)
{
    cast_rays();
    draw_minimap();
    int th = gpu_target_h();
    gpu_draw_string(8, th - 20, "WASD/Arrows:move  ESC:close",
                    COLOR_WHITE, COLOR_DKGREEN);
}

void raycaster_run(void)
{
    raycaster_init();
    u64 last_frame = timer_get_ticks();

    for (;;) {
        int key = input_poll();
        while (key != KEY_NONE) {
            if (key == KEY_ESC) return;
            raycaster_key(key);
            key = input_poll();
        }

        u64 now = timer_get_ticks();
        if ((int)(now - last_frame) >= 3) {
            last_frame = now;
            raycaster_draw();
            gpu_flip();
        }

        sched_yield();
    }
}
