#ifndef DRAGOON_GPU_H
#define DRAGOON_GPU_H

#include "types.h"
#include "font.h"

/* Colors (XRGB8888) */
#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_RED     0x00FF0000
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x000000FF
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_CYAN    0x0000FFFF
#define COLOR_MAGENTA 0x00FF00FF
#define COLOR_GRAY    0x00808080
#define COLOR_DKGRAY  0x00404040
#define COLOR_LTGRAY  0x00C0C0C0
#define COLOR_ORANGE  0x00FF8000
#define COLOR_DKBLUE  0x00000080
#define COLOR_DKGREEN 0x00008000
#define COLOR_DKRED   0x00800000
#define COLOR_BROWN   0x00804000

/* Screen dimensions in characters */
#define SCREEN_COLS  80
#define SCREEN_ROWS  30

/* Initialize GPU with framebuffer pointer. Allocates back buffer. */
int gpu_init(u32 *framebuffer);

/* Drawing functions (all draw to back buffer) */
void gpu_clear(u32 color);
void gpu_pixel(int x, int y, u32 color);
void gpu_fill_rect(int x, int y, int w, int h, u32 color);
void gpu_draw_rect(int x, int y, int w, int h, u32 color);
void gpu_hline(int x, int y, int w, u32 color);
void gpu_vline(int x, int y, int h, u32 color);
void gpu_draw_char(int x, int y, char c, u32 fg, u32 bg);
void gpu_draw_string(int x, int y, const char *s, u32 fg, u32 bg);
void gpu_draw_int(int x, int y, int val, u32 fg, u32 bg);

/* Copy back buffer to framebuffer (call once per frame) */
void gpu_flip(void);

/* Direct access to back buffer (for raycaster column drawing) */
u32 *gpu_backbuf(void);

/* Render target: redirect all drawing to an arbitrary buffer */
void gpu_set_target(u32 *buf, int w, int h);
void gpu_reset_target(void);
int  gpu_target_w(void);
int  gpu_target_h(void);

/* Blit a pixel buffer onto the back buffer (for window compositing) */
void gpu_blit(int dx, int dy, const u32 *src, int sw, int sh, int src_stride);

/* Draw mouse cursor sprite at (x,y) */
void gpu_draw_cursor(int x, int y);

#endif /* DRAGOON_GPU_H */
