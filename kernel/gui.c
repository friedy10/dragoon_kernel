/*
 * Dragoon Microkernel - GUI Shell with Start Menu
 *
 * Non-blocking cooperative desktop. All apps (terminal, games) run as
 * tick-based callbacks driven by the main loop. Multiple apps can be
 * open simultaneously in separate windows.
 */
#include "gui.h"
#include "gpu.h"
#include "font.h"
#include "fb.h"
#include "input.h"
#include "printf.h"
#include "mm.h"
#include "timer.h"
#include "sched.h"
#include "task.h"
#include "wm.h"

/* Game headers (non-blocking API) */
extern void snake_init(void);
extern void snake_key(int key);
extern int  snake_tick(void);
extern void snake_draw(void);

extern void tetris_init(void);
extern void tetris_key(int key);
extern int  tetris_tick(void);
extern void tetris_draw(void);

extern void raycaster_init(void);
extern void raycaster_key(int key);
extern int  raycaster_tick(void);
extern void raycaster_draw(void);

extern void breakout_init(void);
extern void breakout_key(int key);
extern int  breakout_tick(void);
extern void breakout_draw(void);

/* Desktop color */
#define DESKTOP_BG COLOR_DKBLUE

/* Frame pacing: ~30fps = redraw every 3 ticks (at 10ms/tick) */
#define FRAME_TICKS 3

/* App types */
#define APP_NONE      0
#define APP_TERMINAL  1
#define APP_SNAKE     2
#define APP_TETRIS    3
#define APP_RAYCASTER 4
#define APP_BREAKOUT  5

/* Per-window app tracking */
static int win_app_type[WM_MAX_WINDOWS];

/* ---- Start Menu ---- */

#define MENU_ITEMS 5
static const char *menu_labels[MENU_ITEMS] = {
    "Terminal",
    "Snake",
    "Tetris",
    "Raycaster",
    "Breakout"
};
static const int menu_app_types[MENU_ITEMS] = {
    APP_TERMINAL, APP_SNAKE, APP_TETRIS, APP_RAYCASTER, APP_BREAKOUT
};

static int start_menu_open;

#define START_BTN_W  60
#define MENU_ITEM_H  24
#define MENU_W       140

static void draw_start_menu(int mx, int my)
{
    if (!start_menu_open) return;

    int menu_h = MENU_ITEMS * MENU_ITEM_H + 4;
    int menu_x = 0;
    int menu_y = FB_HEIGHT - TASKBAR_H - menu_h;

    /* Background */
    gpu_fill_rect(menu_x, menu_y, MENU_W, menu_h, COLOR_DKGRAY);
    gpu_draw_rect(menu_x, menu_y, MENU_W, menu_h, COLOR_GRAY);

    /* Items */
    for (int i = 0; i < MENU_ITEMS; i++) {
        int iy = menu_y + 2 + i * MENU_ITEM_H;

        /* Highlight on hover */
        if (mx >= menu_x && mx < menu_x + MENU_W &&
            my >= iy && my < iy + MENU_ITEM_H) {
            gpu_fill_rect(menu_x + 1, iy, MENU_W - 2, MENU_ITEM_H, 0x00336699);
            gpu_draw_string(menu_x + 8, iy + 5, menu_labels[i],
                            COLOR_WHITE, 0x00336699);
        } else {
            gpu_draw_string(menu_x + 8, iy + 5, menu_labels[i],
                            COLOR_LTGRAY, COLOR_DKGRAY);
        }
    }
}

/* Returns selected app type or -1 */
static int start_menu_click(int mx, int my)
{
    if (!start_menu_open) return -1;

    int menu_h = MENU_ITEMS * MENU_ITEM_H + 4;
    int menu_x = 0;
    int menu_y = FB_HEIGHT - TASKBAR_H - menu_h;

    if (mx < menu_x || mx >= menu_x + MENU_W ||
        my < menu_y || my >= menu_y + menu_h) {
        start_menu_open = 0;
        return -1;
    }

    for (int i = 0; i < MENU_ITEMS; i++) {
        int iy = menu_y + 2 + i * MENU_ITEM_H;
        if (my >= iy && my < iy + MENU_ITEM_H) {
            start_menu_open = 0;
            return menu_app_types[i];
        }
    }

    start_menu_open = 0;
    return -1;
}

/* ---- Built-in Terminal (non-blocking) ---- */

#define TERM_COLS 60
#define TERM_ROWS 20
#define TERM_PAD  4
#define INPUT_MAX 56

static char term_buf[TERM_ROWS][TERM_COLS + 1];
static int  term_row;
static char term_input[INPUT_MAX + 1];
static int  term_input_len;
static int  term_win_id = -1;

static void term_clear(void)
{
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c <= TERM_COLS; c++)
            term_buf[r][c] = 0;
    term_row = 0;
}

static void term_scroll(void)
{
    for (int r = 0; r < TERM_ROWS - 1; r++)
        for (int c = 0; c <= TERM_COLS; c++)
            term_buf[r][c] = term_buf[r + 1][c];
    for (int c = 0; c <= TERM_COLS; c++)
        term_buf[TERM_ROWS - 1][c] = 0;
    term_row = TERM_ROWS - 1;
}

static void term_putline(const char *s)
{
    if (term_row >= TERM_ROWS)
        term_scroll();
    int c = 0;
    while (*s && c < TERM_COLS)
        term_buf[term_row][c++] = *s++;
    term_buf[term_row][c] = 0;
    term_row++;
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static void term_handle_key(int key)
{
    if (key == KEY_ENTER) {
        term_input[term_input_len] = 0;
        if (str_eq(term_input, "help")) {
            term_putline("Commands: help, clear, ps, mem");
        } else if (str_eq(term_input, "clear")) {
            term_clear();
        } else if (str_eq(term_input, "ps")) {
            term_putline("Tasks:");
            for (int i = 0; i < MAX_TASKS; i++) {
                struct task *t = task_get(i);
                if (t) {
                    char line[TERM_COLS + 1];
                    int pos = 0;
                    line[pos++] = ' '; line[pos++] = ' ';
                    line[pos++] = '[';
                    line[pos++] = '0' + i;
                    line[pos++] = ']';
                    line[pos++] = ' ';
                    for (int j = 0; t->name[j] && pos < TERM_COLS - 10; j++)
                        line[pos++] = t->name[j];
                    line[pos++] = ' ';
                    line[pos++] = '(';
                    const char *states[] = {"dead","ready","running","blocked"};
                    const char *st = (t->state >= 0 && t->state <= 3) ? states[t->state] : "?";
                    while (*st && pos < TERM_COLS - 2)
                        line[pos++] = *st++;
                    line[pos++] = ')';
                    line[pos] = 0;
                    term_putline(line);
                }
            }
        } else if (str_eq(term_input, "mem")) {
            char line[TERM_COLS + 1];
            u64 free_p = mm_get_free_pages();
            u64 total_p = mm_get_total_pages();
            int pos = 0;
            const char *prefix = "Free: ";
            while (*prefix) line[pos++] = *prefix++;
            char num[16]; int ni = 0;
            u64 v = free_p;
            if (v == 0) num[ni++] = '0';
            else while (v > 0) { num[ni++] = '0' + (v % 10); v /= 10; }
            for (int j = ni - 1; j >= 0; j--) line[pos++] = num[j];
            const char *mid = " / ";
            while (*mid) line[pos++] = *mid++;
            ni = 0; v = total_p;
            if (v == 0) num[ni++] = '0';
            else while (v > 0) { num[ni++] = '0' + (v % 10); v /= 10; }
            for (int j = ni - 1; j >= 0; j--) line[pos++] = num[j];
            const char *suffix = " pages (4KB each)";
            while (*suffix && pos < TERM_COLS) line[pos++] = *suffix++;
            line[pos] = 0;
            term_putline(line);
        } else if (term_input_len > 0) {
            char line[TERM_COLS + 1];
            int pos = 0;
            const char *p = "Unknown: ";
            while (*p && pos < TERM_COLS - INPUT_MAX) line[pos++] = *p++;
            for (int j = 0; j < term_input_len && pos < TERM_COLS; j++)
                line[pos++] = term_input[j];
            line[pos] = 0;
            term_putline(line);
        }
        term_input_len = 0;
    } else if (key == KEY_BACKSPACE) {
        if (term_input_len > 0) term_input_len--;
    } else if (key >= 32 && key < 127 && term_input_len < INPUT_MAX) {
        term_input[term_input_len++] = (char)key;
    }
}

static void term_render_to_target(void)
{
    gpu_clear(COLOR_BLACK);

    /* Title */
    gpu_draw_string(TERM_PAD, TERM_PAD,
                    "Dragoon Terminal", COLOR_GREEN, COLOR_BLACK);

    /* Text rows */
    for (int r = 0; r < TERM_ROWS; r++) {
        gpu_draw_string(TERM_PAD,
                        TERM_PAD + 16 + r * FONT_HEIGHT,
                        term_buf[r], COLOR_LTGRAY, COLOR_BLACK);
    }

    /* Input line */
    int tw = gpu_target_w();
    int iy = TERM_PAD + 16 + TERM_ROWS * FONT_HEIGHT + 4;
    gpu_fill_rect(0, iy, tw, FONT_HEIGHT, COLOR_DKGRAY);
    gpu_draw_char(TERM_PAD, iy, '>', COLOR_GREEN, COLOR_DKGRAY);
    gpu_draw_char(TERM_PAD + FONT_WIDTH, iy, ' ', COLOR_GREEN, COLOR_DKGRAY);

    term_input[term_input_len] = 0;
    gpu_draw_string(TERM_PAD + FONT_WIDTH * 2, iy,
                    term_input, COLOR_WHITE, COLOR_DKGRAY);
    gpu_draw_char(TERM_PAD + FONT_WIDTH * (2 + term_input_len), iy,
                  '_', COLOR_WHITE, COLOR_DKGRAY);
}

/* ---- App dispatch ---- */

static void app_init(int win_id, int app_type)
{
    u32 *content = wm_get_content(win_id);
    int cw = wm_get_content_w(win_id);
    int ch = wm_get_content_h(win_id);
    if (!content) return;

    gpu_set_target(content, cw, ch);

    switch (app_type) {
    case APP_TERMINAL:
        term_clear();
        term_input_len = 0;
        term_putline("Dragoon Terminal v0.2");
        term_putline("Type 'help' for commands.");
        term_putline("");
        break;
    case APP_SNAKE:     snake_init();     break;
    case APP_TETRIS:    tetris_init();    break;
    case APP_RAYCASTER: raycaster_init(); break;
    case APP_BREAKOUT:  breakout_init();  break;
    }

    gpu_reset_target();
}

static void app_key(int win_id, int key)
{
    int app = win_app_type[win_id];
    switch (app) {
    case APP_TERMINAL:  term_handle_key(key); break;
    case APP_SNAKE:     snake_key(key);       break;
    case APP_TETRIS:    tetris_key(key);      break;
    case APP_RAYCASTER: raycaster_key(key);   break;
    case APP_BREAKOUT:  breakout_key(key);    break;
    }
}

static void app_tick(int win_id)
{
    int app = win_app_type[win_id];
    switch (app) {
    case APP_SNAKE:     snake_tick();     break;
    case APP_TETRIS:    tetris_tick();    break;
    case APP_RAYCASTER: raycaster_tick(); break;
    case APP_BREAKOUT:  breakout_tick();  break;
    }
}

static void app_render(int win_id)
{
    u32 *content = wm_get_content(win_id);
    int cw = wm_get_content_w(win_id);
    int ch = wm_get_content_h(win_id);
    if (!content) return;

    gpu_set_target(content, cw, ch);

    int app = win_app_type[win_id];
    switch (app) {
    case APP_TERMINAL:  term_render_to_target(); break;
    case APP_SNAKE:     snake_draw();            break;
    case APP_TETRIS:    tetris_draw();           break;
    case APP_RAYCASTER: raycaster_draw();        break;
    case APP_BREAKOUT:  breakout_draw();         break;
    }

    gpu_reset_target();
}

/* ---- Window creation for apps ---- */

static int find_existing_app_window(int app_type)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (win_app_type[i] == app_type && wm_get_window(i))
            return i;
    }
    return -1;
}

static void open_app(int app_type)
{
    /* If already open, just focus it */
    int existing = find_existing_app_window(app_type);
    if (existing >= 0) {
        wm_focus_window(existing);
        return;
    }

    const char *title = "App";
    int w = 500, h = 370;
    int x = 80, y = 30;

    switch (app_type) {
    case APP_TERMINAL:
        title = "Terminal";
        w = 500; h = 370;
        x = 120; y = 40;
        break;
    case APP_SNAKE:
        title = "Snake";
        w = 612; h = 472;
        x = 10;  y = 4;
        break;
    case APP_TETRIS:
        title = "Tetris";
        w = 420; h = 440;
        x = 100; y = 10;
        break;
    case APP_RAYCASTER:
        title = "Raycaster";
        w = 500; h = 400;
        x = 60;  y = 20;
        break;
    case APP_BREAKOUT:
        title = "Breakout";
        w = 620; h = 440;
        x = 10;  y = 10;
        break;
    }

    int win_id = wm_create_window(title, x, y, w, h);
    if (win_id < 0) return;

    win_app_type[win_id] = app_type;
    app_init(win_id, app_type);
}

/* ---- Main Desktop Loop ---- */

static void wait_frame(u64 *last_frame)
{
    for (;;) {
        u64 now = timer_get_ticks();
        if ((int)(now - *last_frame) >= FRAME_TICKS) {
            *last_frame = now;
            return;
        }
        sched_yield();
    }
}

void gui_main(void)
{
    kprintf("[gui] starting GUI\n");

    /* Initialize framebuffer */
    if (fb_init() < 0) {
        kprintf("[gui] framebuffer init failed, exiting\n");
        return;
    }

    u32 *buffer = fb_get_buffer();
    if (!buffer) {
        kprintf("[gui] no framebuffer, exiting\n");
        return;
    }

    gpu_init(buffer);
    kprintf("[gui] GPU initialized (double-buffered)\n");

    /* Initialize input (probes virtio keyboard/mouse) */
    input_init();

    /* Initialize window manager */
    wm_init();

    kprintf("[gui] desktop ready\n");

    start_menu_open = 0;
    term_win_id = -1;
    term_input_len = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        win_app_type[i] = APP_NONE;

    int prev_left = 0;
    u64 last_frame = timer_get_ticks();

    for (;;) {
        /* Process mouse */
        int mx, my, mb;
        mouse_get_state(&mx, &my, &mb);

        int left_click = (mb & MOUSE_BTN_LEFT) && !prev_left;
        prev_left = mb & MOUSE_BTN_LEFT;

        /* Left click handling */
        if (left_click) {
            /* Check start menu first */
            if (start_menu_open) {
                int app = start_menu_click(mx, my);
                if (app > 0) {
                    open_app(app);
                }
                goto done_click;
            }

            /* Check start button in taskbar */
            if (my >= FB_HEIGHT - TASKBAR_H && mx < START_BTN_W) {
                start_menu_open = !start_menu_open;
                goto done_click;
            }

            /* Close menu if clicking elsewhere */
            if (start_menu_open) {
                start_menu_open = 0;
                goto done_click;
            }

            /* Let WM handle window clicks */
            int result = wm_handle_mouse(mx, my, mb);

            /* Check for window close signals */
            if (result <= -100) {
                int close_id = -(result + 100);
                int app = win_app_type[close_id];
                win_app_type[close_id] = APP_NONE;
                if (app == APP_TERMINAL)
                    term_win_id = -1;
                wm_destroy_window(close_id);
            }
        } else {
            /* Let WM handle dragging */
            wm_handle_mouse(mx, my, mb);
        }
done_click:

        /* Process keyboard: send to focused window's app */
        {
            int key = input_poll();
            while (key != KEY_NONE) {
                int focused = wm_get_focused();
                if (focused >= 0 && win_app_type[focused] != APP_NONE) {
                    /* ESC closes the window */
                    if (key == KEY_ESC) {
                        int app = win_app_type[focused];
                        win_app_type[focused] = APP_NONE;
                        if (app == APP_TERMINAL)
                            term_win_id = -1;
                        wm_destroy_window(focused);
                    } else {
                        app_key(focused, key);
                    }
                }
                key = input_poll();
            }
        }

        /* Tick all open apps */
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (win_app_type[i] != APP_NONE && wm_get_window(i))
                app_tick(i);
        }

        /* Render all open apps into their window buffers */
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (win_app_type[i] != APP_NONE && wm_get_window(i))
                app_render(i);
        }

        /* Composite frame */
        wm_composite();

        /* Draw start menu on top of windows */
        draw_start_menu(mx, my);

        /* Cursor on top */
        gpu_draw_cursor(mx, my);

        gpu_flip();
        wait_frame(&last_frame);
    }
}
