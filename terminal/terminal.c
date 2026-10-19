#include "terminal/terminal.h"
#include "graphics/font.h"
#include "drivers/framebuffer.h"

static uint32_t g_fg;
static uint32_t g_bg;
static uint32_t g_x;        /* pixel x del siguiente glifo */
static uint32_t g_y;        /* pixel y del siguiente glifo */
static uint32_t g_cw;       /* ancho de celda en pixeles */
static uint32_t g_ch;       /* alto de celda en pixeles */
static uint32_t g_ct;       /* grosor del cursor */
static int      g_scale;    /* escala porcentual */
static int      g_cursor_on;

static void cursor_erase(void) {
    if (!g_cursor_on) return;
    fb_fill_rect(g_x, g_y + g_ch - g_ct, g_cw, g_ct, g_bg);
    g_cursor_on = 0;
}

static void cursor_draw(void) {
    fb_fill_rect(g_x, g_y + g_ch - g_ct, g_cw, g_ct, g_fg);
    g_cursor_on = 1;
}

static void advance_line(void) {
    g_x = 0;
    g_y += g_ch;
    if (g_y + g_ch > fb_height()) {
        fb_scroll(g_ch, g_bg);
        g_y -= g_ch;
    }
}

void term_init(uint32_t fg, uint32_t bg) {
    g_scale = TERM_SCALE;
    g_cw = (uint32_t)font_cell_w(g_scale);
    g_ch = (uint32_t)font_cell_h(g_scale);
    g_ct = (g_ch >= 4) ? g_ch / 4 : 1;
    g_fg = fg;
    g_bg = bg;
    g_x = 0;
    g_y = 0;
    g_cursor_on = 0;
    fb_clear(bg);
}

void term_set_color(uint32_t fg) { g_fg = fg; cursor_erase(); cursor_draw(); }
void term_set_bg(uint32_t bg)    { g_bg = bg; }

void term_clear(void) {
    cursor_erase();
    fb_clear(g_bg);
    g_x = 0;
    g_y = 0;
    cursor_draw();
}

void term_newline(void) {
    cursor_erase();
    advance_line();
    cursor_draw();
}

void term_putchar(char c) {
    cursor_erase();

    if (c == '\n') {
        advance_line();
    } else if (c == '\r') {
        g_x = 0;
    } else if (c == '\b') {
        if (g_x >= g_cw) {
            g_x -= g_cw;
            font_draw_char(g_x, g_y, ' ', g_fg, g_bg, g_scale);
        }
    } else if (c == '\t') {
        uint32_t stop = g_cw * 4;
        g_x = ((g_x / stop) + 1) * stop;
        if (g_x + g_cw > fb_width()) advance_line();
    } else {
        if (g_x + g_cw > fb_width()) advance_line();
        font_draw_char(g_x, g_y, c, g_fg, g_bg, g_scale);
        g_x += g_cw;
    }

    cursor_draw();
}

void term_write(const char *s) {
    while (*s) term_putchar(*s++);
}

void term_draw_text_at(uint32_t x, uint32_t y, const char *s, uint32_t color) {
    uint32_t cx = x;
    while (*s) {
        char c = *s++;
        if (c == '\n') {
            cx = x;
            y += g_ch;
            continue;
        }
        font_draw_char(cx, y, c, color, g_bg, g_scale);
        cx += g_cw;
    }
}

void term_set_cursor_px(uint32_t x, uint32_t y) {
    cursor_erase();
    g_x = x;
    g_y = y;
    cursor_draw();
}

uint32_t term_color(void)  { return g_fg; }
uint32_t term_bg(void)     { return g_bg; }
uint32_t term_cell_w(void) { return g_cw; }
uint32_t term_cell_h(void) { return g_ch; }
int      term_scale(void)  { return g_scale; }
