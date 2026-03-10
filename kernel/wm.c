/*
 * Dragoon Microkernel - TWM-style Window Manager
 *
 * Manages windows with title bars, dragging, focus, close buttons.
 * Composites all windows onto the GPU back buffer each frame.
 * Mouse-driven: click to focus, drag title bars, click [X] to close.
 */
#include "wm.h"
#include "gpu.h"
#include "fb.h"
#include "font.h"
#include "mm.h"
#include "input.h"
#include "printf.h"

extern void *memset(void *s, int c, u64 n);

static struct wm_window windows[WM_MAX_WINDOWS];
static int z_order[WM_MAX_WINDOWS]; /* indices into windows[], back-to-front */
static int z_count;                  /* number of visible windows */

static int prev_buttons;

/* Color scheme */
#define TITLE_BG_FOCUSED   0x00336699
#define TITLE_BG_UNFOCUSED 0x00606060
#define TITLE_FG           COLOR_WHITE
#define CLOSE_BG           0x00CC3333
#define CLOSE_FG           COLOR_WHITE
#define BORDER_COLOR       0x00808080
#define DESKTOP_BG         COLOR_DKBLUE
#define TASKBAR_BG         0x00333333
#define TASKBAR_FG         COLOR_LTGRAY
#define TASKBAR_ACTIVE_BG  0x00336699

void wm_init(void)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].visible = 0;
        windows[i].content = NULL;
    }
    z_count = 0;
    prev_buttons = 0;
}

int wm_create_window(const char *title, int x, int y, int w, int h)
{
    int id = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!windows[i].visible) {
            id = i;
            break;
        }
    }
    if (id < 0) return -1;

    struct wm_window *win = &windows[id];

    /* Copy title */
    int ti = 0;
    while (title[ti] && ti < 31) {
        win->title[ti] = title[ti];
        ti++;
    }
    win->title[ti] = 0;

    win->x = x;
    win->y = y;
    win->content_w = w;
    win->content_h = h;
    win->w = w + 2;                /* 1px border each side */
    win->h = h + TITLEBAR_H + 2;  /* title bar + 1px border top/bottom */

    /* Allocate content buffer */
    u64 buf_size = (u64)w * (u64)h * 4;
    u64 pages = (buf_size + PAGE_SIZE - 1) / PAGE_SIZE;
    win->content = (u32 *)pages_alloc(pages);
    if (!win->content) {
        kprintf("[wm] failed to alloc window buffer\n");
        return -1;
    }

    /* Fill with black */
    memset(win->content, 0, buf_size);

    win->visible = 1;
    win->focused = 0;
    win->dragging = 0;
    win->needs_redraw = 1;

    /* Add to z-order (on top) */
    z_order[z_count] = id;
    z_count++;

    /* Focus it */
    wm_focus_window(id);

    kprintf("[wm] created window %d: '%s' (%dx%d at %d,%d)\n",
            id, title, w, h, x, y);
    return id;
}

void wm_destroy_window(int id)
{
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].visible)
        return;

    struct wm_window *win = &windows[id];

    /* Free content buffer */
    if (win->content) {
        u64 buf_size = (u64)win->content_w * (u64)win->content_h * 4;
        u64 pages = (buf_size + PAGE_SIZE - 1) / PAGE_SIZE;
        pages_free(win->content, pages);
        win->content = NULL;
    }

    win->visible = 0;
    win->focused = 0;

    /* Remove from z-order */
    int found = 0;
    for (int i = 0; i < z_count; i++) {
        if (z_order[i] == id)
            found = 1;
        if (found && i + 1 < z_count)
            z_order[i] = z_order[i + 1];
    }
    if (found) z_count--;

    /* Focus topmost window */
    if (z_count > 0) {
        wm_focus_window(z_order[z_count - 1]);
    }
}

u32 *wm_get_content(int id)
{
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].visible)
        return NULL;
    return windows[id].content;
}

int wm_get_content_w(int id)
{
    if (id < 0 || id >= WM_MAX_WINDOWS) return 0;
    return windows[id].content_w;
}

int wm_get_content_h(int id)
{
    if (id < 0 || id >= WM_MAX_WINDOWS) return 0;
    return windows[id].content_h;
}

struct wm_window *wm_get_window(int id)
{
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].visible)
        return NULL;
    return &windows[id];
}

void wm_focus_window(int id)
{
    if (id < 0 || id >= WM_MAX_WINDOWS || !windows[id].visible)
        return;

    /* Unfocus all */
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        windows[i].focused = 0;

    windows[id].focused = 1;

    /* Move to top of z-order */
    int pos = -1;
    for (int i = 0; i < z_count; i++) {
        if (z_order[i] == id) {
            pos = i;
            break;
        }
    }
    if (pos >= 0 && pos < z_count - 1) {
        for (int i = pos; i < z_count - 1; i++)
            z_order[i] = z_order[i + 1];
        z_order[z_count - 1] = id;
    }
}

int wm_get_focused(void)
{
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].visible && windows[i].focused)
            return i;
    }
    return -1;
}

int wm_window_count(void)
{
    int count = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].visible) count++;
    }
    return count;
}

static void draw_window(struct wm_window *win)
{
    int x = win->x;
    int y = win->y;
    int w = win->w;
    int h = win->h;

    /* Border */
    gpu_draw_rect(x, y, w, h, BORDER_COLOR);

    /* Title bar */
    u32 title_bg = win->focused ? TITLE_BG_FOCUSED : TITLE_BG_UNFOCUSED;
    gpu_fill_rect(x + 1, y + 1, w - 2, TITLEBAR_H, title_bg);
    gpu_draw_string(x + 4, y + 3, win->title, TITLE_FG, title_bg);

    /* Close button [X] */
    int cx = x + w - CLOSE_BTN_W - 2;
    int cy = y + 2;
    gpu_fill_rect(cx, cy, CLOSE_BTN_W, TITLEBAR_H - 4, CLOSE_BG);
    gpu_draw_char(cx + 4, cy + 1, 'X', CLOSE_FG, CLOSE_BG);

    /* Client area: blit content buffer */
    int client_x = x + 1;
    int client_y = y + TITLEBAR_H + 1;
    if (win->content) {
        gpu_blit(client_x, client_y, win->content,
                 win->content_w, win->content_h, win->content_w);
    }
}

#define START_BTN_W 60

static void draw_taskbar(void)
{
    int ty = FB_HEIGHT - TASKBAR_H;
    gpu_fill_rect(0, ty, FB_WIDTH, TASKBAR_H, TASKBAR_BG);
    gpu_hline(0, ty, FB_WIDTH, BORDER_COLOR);

    /* Start button */
    gpu_fill_rect(0, ty + 1, START_BTN_W, TASKBAR_H - 1, 0x00336699);
    gpu_draw_string(6, ty + 5, "Start", COLOR_WHITE, 0x00336699);

    /* Draw window buttons in taskbar */
    int bx = START_BTN_W + 4;
    for (int i = 0; i < z_count; i++) {
        int id = z_order[i];
        struct wm_window *win = &windows[id];
        if (!win->visible) continue;

        int len = 0;
        while (win->title[len]) len++;
        int bw = (len + 2) * FONT_WIDTH + 8;

        u32 bg = win->focused ? TASKBAR_ACTIVE_BG : TASKBAR_BG;
        gpu_fill_rect(bx, ty + 2, bw, TASKBAR_H - 4, bg);
        gpu_draw_string(bx + 4, ty + 5, win->title, TASKBAR_FG, bg);

        bx += bw + 4;
    }

    /* Right side: "DRAGOON" label */
    gpu_draw_string(FB_WIDTH - 8 * FONT_WIDTH - 4, ty + 5,
                    "DRAGOON", COLOR_GRAY, TASKBAR_BG);
}

void wm_composite(void)
{
    /* Desktop background */
    gpu_clear(DESKTOP_BG);

    /* Desktop watermark */
    int wx = (FB_WIDTH - 10 * FONT_WIDTH) / 2;
    int wy = (FB_HEIGHT - TASKBAR_H) / 2 - 8;
    gpu_draw_string(wx, wy, "DRAGOON OS", 0x00102040, DESKTOP_BG);

    /* Draw windows back-to-front */
    for (int i = 0; i < z_count; i++) {
        int id = z_order[i];
        if (windows[id].visible)
            draw_window(&windows[id]);
    }

    /* Taskbar */
    draw_taskbar();
}

int wm_handle_mouse(int mx, int my, int buttons)
{
    int clicked = (buttons & MOUSE_BTN_LEFT) && !(prev_buttons & MOUSE_BTN_LEFT);
    int released = !(buttons & MOUSE_BTN_LEFT) && (prev_buttons & MOUSE_BTN_LEFT);
    prev_buttons = buttons;

    /* Handle active drags */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].dragging) {
            if (released) {
                windows[i].dragging = 0;
            } else {
                windows[i].x = mx - windows[i].drag_ox;
                windows[i].y = my - windows[i].drag_oy;
                /* Clamp to keep title bar visible */
                if (windows[i].y < 0) windows[i].y = 0;
                if (windows[i].y > FB_HEIGHT - TASKBAR_H - TITLEBAR_H)
                    windows[i].y = FB_HEIGHT - TASKBAR_H - TITLEBAR_H;
            }
            return -1;
        }
    }

    if (!clicked)
        return -1;

    /* Check taskbar clicks (skip start button area handled by gui.c) */
    if (my >= FB_HEIGHT - TASKBAR_H) {
        if (mx < START_BTN_W) return -2; /* start button: signal to gui.c */
        int bx = START_BTN_W + 4;
        for (int i = 0; i < z_count; i++) {
            int id = z_order[i];
            struct wm_window *win = &windows[id];
            if (!win->visible) continue;
            int len = 0;
            while (win->title[len]) len++;
            int bw = (len + 2) * FONT_WIDTH + 8;
            if (mx >= bx && mx < bx + bw) {
                wm_focus_window(id);
                return -1;
            }
            bx += bw + 4;
        }
        return -1;
    }

    /* Check windows top-to-bottom (reverse z-order) for clicks */
    for (int i = z_count - 1; i >= 0; i--) {
        int id = z_order[i];
        struct wm_window *win = &windows[id];
        if (!win->visible) continue;

        /* Hit test */
        if (mx < win->x || mx >= win->x + win->w ||
            my < win->y || my >= win->y + win->h)
            continue;

        /* Focus this window */
        wm_focus_window(id);

        /* Check close button */
        int cx = win->x + win->w - CLOSE_BTN_W - 2;
        int cy = win->y + 2;
        if (mx >= cx && mx < cx + CLOSE_BTN_W &&
            my >= cy && my < cy + TITLEBAR_H - 4) {
            return -(id + 100); /* special: close signal = -(id+100) */
        }

        /* Check title bar drag */
        if (my < win->y + TITLEBAR_H + 1) {
            win->dragging = 1;
            win->drag_ox = mx - win->x;
            win->drag_oy = my - win->y;
            return -1;
        }

        /* Click in client area */
        return id;
    }

    return -1; /* click on desktop */
}
