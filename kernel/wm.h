#ifndef DRAGOON_WM_H
#define DRAGOON_WM_H

#include "types.h"

#define WM_MAX_WINDOWS  8
#define TITLEBAR_H      20
#define TASKBAR_H       24
#define CLOSE_BTN_W     16

/* Window structure */
struct wm_window {
    int x, y;              /* outer position (includes title bar) */
    int w, h;              /* outer size (includes title bar) */
    char title[32];
    u32 *content;          /* pixel buffer for client area */
    int content_w;
    int content_h;
    int visible;
    int focused;
    int dragging;
    int drag_ox, drag_oy;  /* offset from window corner to drag start */
    int needs_redraw;       /* flag for app to signal content changed */
};

/* Initialize window manager */
void wm_init(void);

/* Create a window. Returns window id (0-7) or -1 on failure.
 * w, h are CLIENT area dimensions; title bar is added on top. */
int  wm_create_window(const char *title, int x, int y, int w, int h);

/* Destroy a window */
void wm_destroy_window(int id);

/* Get the client area pixel buffer to draw into */
u32 *wm_get_content(int id);
int  wm_get_content_w(int id);
int  wm_get_content_h(int id);

/* Get window by id (NULL if invalid/inactive) */
struct wm_window *wm_get_window(int id);

/* Compositing: draw all windows, taskbar, cursor to back buffer */
void wm_composite(void);

/* Process mouse input for window management (drag, focus, close).
 * Returns id of window whose content area was clicked, or -1. */
int  wm_handle_mouse(int mx, int my, int buttons);

/* Get the currently focused window id, or -1 */
int  wm_get_focused(void);

/* Bring window to front and focus it */
void wm_focus_window(int id);

/* Get number of active windows */
int  wm_window_count(void);

#endif /* DRAGOON_WM_H */
