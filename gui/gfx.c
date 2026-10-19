#include "gui/gfx.h"
#include "drivers/framebuffer.h"
#include "graphics/font.h"
#include "kernel/heap.h"
#include "kernel/lib.h"

static uint32_t *g_buf;
static int g_w, g_h;

/* Rectangulo de recorte (exclusivo en x1/y1). */
static int g_cx0, g_cy0, g_cx1, g_cy1;

uint32_t gfx_map_rgb(uint32_t rgb) { return rgb & 0x00FFFFFFu; }

void gfx_init(void) {
    g_w = (int)fb_width();
    g_h = (int)fb_height();
    g_buf = (uint32_t *)kmalloc((size_t)g_w * g_h * 4);
    gfx_clip_reset();
    if (g_buf) gfx_clear(0x00101828u);
}

int gfx_ok(void) { return g_buf != 0; }

void gfx_clip_reset(void) {
    g_cx0 = 0; g_cy0 = 0;
    g_cx1 = g_w; g_cy1 = g_h;
}

void gfx_clip_set(int x, int y, int w, int h) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > g_w) x1 = g_w;
    int y1 = y + h; if (y1 > g_h) y1 = g_h;

    /* Interseccion con el recorte actual. */
    if (x0 < g_cx0) x0 = g_cx0;
    if (y0 < g_cy0) y0 = g_cy0;
    if (x1 > g_cx1) x1 = g_cx1;
    if (y1 > g_cy1) y1 = g_cy1;

    g_cx0 = x0; g_cy0 = y0; g_cx1 = x1; g_cy1 = y1;
}

void gfx_clear(uint32_t color) {
    gfx_fill(0, 0, g_w, g_h, color);
}

void gfx_pixel(int x, int y, uint32_t color) {
    if (x < g_cx0 || x >= g_cx1 || y < g_cy0 || y >= g_cy1) return;
    g_buf[(uint64_t)y * g_w + x] = gfx_map_rgb(color);
}

void gfx_fill(int x, int y, int w, int h, uint32_t color) {
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < g_cx0) x0 = g_cx0;
    if (y0 < g_cy0) y0 = g_cy0;
    if (x1 > g_cx1) x1 = g_cx1;
    if (y1 > g_cy1) y1 = g_cy1;
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t c = gfx_map_rgb(color);
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = g_buf + (uint64_t)yy * g_w;
        for (int xx = x0; xx < x1; xx++) row[xx] = c;
    }
}

void gfx_frame(int x, int y, int w, int h, uint32_t color) {
    gfx_fill(x, y, w, 1, color);
    gfx_fill(x, y + h - 1, w, 1, color);
    gfx_fill(x, y, 1, h, color);
    gfx_fill(x + w - 1, y, 1, h, color);
}

void gfx_vgradient(int x, int y, int w, int h, uint32_t arriba, uint32_t abajo) {
    if (h <= 0) return;
    int r0 = (arriba >> 16) & 0xFF, g0 = (arriba >> 8) & 0xFF, b0 = arriba & 0xFF;
    int r1 = (abajo  >> 16) & 0xFF, g1 = (abajo  >> 8) & 0xFF, b1 = abajo  & 0xFF;
    for (int i = 0; i < h; i++) {
        int r = r0 + (r1 - r0) * i / h;
        int g = g0 + (g1 - g0) * i / h;
        int b = b0 + (b1 - b0) * i / h;
        gfx_fill(x, y + i, w, 1, ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
    }
}

int gfx_text_h(int scale) { return font_cell_h(scale); }

int gfx_text_w(const char *s, int scale) {
    return (int)strlen(s) * font_cell_w(scale);
}

void gfx_char(int x, int y, char c, uint32_t fg, uint32_t bg, int scale) {
    int tw = font_cell_w(scale);
    int th = font_cell_h(scale);
    const uint8_t *g = font_glyph(c);

    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = g[row];
        int y0 = y + row * th / FONT_H;
        int y1 = y + (row + 1) * th / FONT_H;
        if (y1 == y0) continue;
        for (int col = 0; col < FONT_W; col++) {
            int x0 = x + col * tw / FONT_W;
            int x1 = x + (col + 1) * tw / FONT_W;
            if (x1 == x0) continue;
            int set = (bits & (1u << col)) != 0;
            if (set)      gfx_fill(x0, y0, x1 - x0, y1 - y0, fg);
            else if (bg != 0xFFFFFFFF) gfx_fill(x0, y0, x1 - x0, y1 - y0, bg);
        }
    }
}

void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale) {
    int cw = font_cell_w(scale);
    int cx = x;
    while (*s) {
        gfx_char(cx, y, *s, fg, bg, scale);
        cx += cw;
        s++;
    }
}

void gfx_present(void) {
    if (!g_buf) return;
    if (fb_bpp() == 32) {
        uint8_t *dst = (uint8_t *)(uintptr_t)fb_addr();
        uint32_t pitch = fb_pitch();
        for (int y = 0; y < g_h; y++)
            memcpy(dst + (uint64_t)y * pitch, g_buf + (uint64_t)y * g_w,
                   (size_t)g_w * 4);
    } else {
        for (int y = 0; y < g_h; y++)
            for (int x = 0; x < g_w; x++)
                fb_put_pixel((uint32_t)x, (uint32_t)y, g_buf[(uint64_t)y * g_w + x]);
    }
}
