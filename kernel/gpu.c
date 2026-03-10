/*
 * Dragoon Microkernel - Graphics Primitives
 *
 * Double-buffered software rendering to XRGB8888 framebuffer.
 * All drawing goes to a back buffer; gpu_flip() copies to the real FB.
 * Supports render target redirection for windowed app rendering.
 */
#include "gpu.h"
#include "fb.h"
#include "font.h"
#include "mm.h"
#include "printf.h"

static u32 *frontbuf;  /* actual framebuffer (ramfb) */
static u32 *backbuf;   /* off-screen back buffer */

/* Current render target (defaults to backbuf/FB_WIDTH/FB_HEIGHT) */
static u32 *cur_buf;
static int  cur_w;
static int  cur_h;

extern void *memcpy(void *dest, const void *src, u64 n);

int gpu_init(u32 *framebuffer)
{
    frontbuf = framebuffer;

    /* Allocate back buffer (same size as framebuffer) */
    u64 fb_pages = (FB_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    backbuf = (u32 *)pages_alloc(fb_pages);
    if (!backbuf) {
        kprintf("[gpu] failed to allocate back buffer\n");
        /* Fall back to direct rendering */
        backbuf = framebuffer;
    }

    cur_buf = backbuf;
    cur_w = FB_WIDTH;
    cur_h = FB_HEIGHT;

    return 0;
}

void gpu_set_target(u32 *buf, int w, int h)
{
    cur_buf = buf;
    cur_w = w;
    cur_h = h;
}

void gpu_reset_target(void)
{
    cur_buf = backbuf;
    cur_w = FB_WIDTH;
    cur_h = FB_HEIGHT;
}

int gpu_target_w(void) { return cur_w; }
int gpu_target_h(void) { return cur_h; }

void gpu_flip(void)
{
    if (backbuf != frontbuf) {
        memcpy(frontbuf, backbuf, FB_SIZE);
    }
}

u32 *gpu_backbuf(void)
{
    return cur_buf;
}

void gpu_clear(u32 color)
{
    for (int i = 0; i < cur_w * cur_h; i++)
        cur_buf[i] = color;
}

void gpu_pixel(int x, int y, u32 color)
{
    if (x >= 0 && x < cur_w && y >= 0 && y < cur_h)
        cur_buf[y * cur_w + x] = color;
}

void gpu_fill_rect(int x, int y, int w, int h, u32 color)
{
    /* Clip */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > cur_w ? cur_w : x + w;
    int y1 = y + h > cur_h ? cur_h : y + h;

    for (int j = y0; j < y1; j++) {
        u32 *row = &cur_buf[j * cur_w];
        for (int i = x0; i < x1; i++)
            row[i] = color;
    }
}

void gpu_draw_rect(int x, int y, int w, int h, u32 color)
{
    gpu_hline(x, y, w, color);
    gpu_hline(x, y + h - 1, w, color);
    gpu_vline(x, y, h, color);
    gpu_vline(x + w - 1, y, h, color);
}

void gpu_hline(int x, int y, int w, u32 color)
{
    if (y < 0 || y >= cur_h) return;
    int x0 = x < 0 ? 0 : x;
    int x1 = x + w > cur_w ? cur_w : x + w;
    u32 *row = &cur_buf[y * cur_w];
    for (int i = x0; i < x1; i++)
        row[i] = color;
}

void gpu_vline(int x, int y, int h, u32 color)
{
    if (x < 0 || x >= cur_w) return;
    int y0 = y < 0 ? 0 : y;
    int y1 = y + h > cur_h ? cur_h : y + h;
    for (int j = y0; j < y1; j++)
        cur_buf[j * cur_w + x] = color;
}

void gpu_draw_char(int x, int y, char c, u32 fg, u32 bg)
{
    const u8 *glyph = font_get_glyph(c);
    for (int row = 0; row < FONT_HEIGHT; row++) {
        int py = y + row;
        if (py < 0 || py >= cur_h) continue;
        u8 bits = glyph[row];
        u32 *rowp = &cur_buf[py * cur_w];
        for (int col = 0; col < FONT_WIDTH; col++) {
            int px = x + col;
            if (px >= 0 && px < cur_w)
                rowp[px] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void gpu_draw_string(int x, int y, const char *s, u32 fg, u32 bg)
{
    while (*s) {
        gpu_draw_char(x, y, *s, fg, bg);
        x += FONT_WIDTH;
        s++;
    }
}

void gpu_draw_int(int x, int y, int val, u32 fg, u32 bg)
{
    char buf[16];
    int neg = 0;
    int i = 0;

    if (val < 0) {
        neg = 1;
        val = -val;
    }
    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            buf[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    if (neg) buf[i++] = '-';

    for (int j = i - 1; j >= 0; j--) {
        gpu_draw_char(x, y, buf[j], fg, bg);
        x += FONT_WIDTH;
    }
}

void gpu_blit(int dx, int dy, const u32 *src, int sw, int sh, int src_stride)
{
    for (int j = 0; j < sh; j++) {
        int py = dy + j;
        if (py < 0 || py >= cur_h) continue;
        const u32 *srow = &src[j * src_stride];
        u32 *drow = &cur_buf[py * cur_w];
        for (int i = 0; i < sw; i++) {
            int px = dx + i;
            if (px >= 0 && px < cur_w)
                drow[px] = srow[i];
        }
    }
}

/* 12x16 mouse cursor bitmap (1=white, 2=black outline, 0=transparent) */
static const u8 cursor_data[16][12] = {
    {2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,1,1,1,1,2,0,0,0},
    {2,1,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,2,2,2,2,0,0},
    {2,1,1,2,1,1,2,0,0,0,0,0},
    {2,1,2,0,2,1,1,2,0,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0,0},
    {0,0,0,0,0,2,1,1,2,0,0,0},
    {0,0,0,0,0,2,2,2,0,0,0,0},
};

void gpu_draw_cursor(int x, int y)
{
    for (int j = 0; j < 16; j++) {
        int py = y + j;
        if (py < 0 || py >= cur_h) continue;
        u32 *row = &cur_buf[py * cur_w];
        for (int i = 0; i < 12; i++) {
            int px = x + i;
            if (px < 0 || px >= cur_w) continue;
            u8 v = cursor_data[j][i];
            if (v == 1)
                row[px] = COLOR_WHITE;
            else if (v == 2)
                row[px] = COLOR_BLACK;
        }
    }
}
