#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint64_t addr;      /* direccion fisica/virtual del framebuffer */
    uint32_t width;     /* ancho en pixeles */
    uint32_t height;    /* alto en pixeles */
    uint32_t pitch;     /* bytes por linea */
    uint8_t  bpp;       /* bits por pixel */
    uint8_t  r_pos, r_size;
    uint8_t  g_pos, g_size;
    uint8_t  b_pos, b_size;
} fb_info_t;

/* Colores en formato 0xRRGGBB (se empaquetan segun las mascaras del hardware) */
#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define COLOR_BLACK  RGB(0x00, 0x00, 0x00)
#define COLOR_WHITE  RGB(0xFF, 0xFF, 0xFF)
#define COLOR_GRAY   RGB(0xAA, 0xAA, 0xAA)
#define COLOR_GREEN  RGB(0x00, 0xCC, 0x44)
#define COLOR_YELLOW RGB(0xFF, 0xFF, 0x33)
#define COLOR_PINK   RGB(0xFF, 0x55, 0xAA)
#define COLOR_RED    RGB(0xFF, 0x33, 0x33)
#define COLOR_CYAN   RGB(0x33, 0xFF, 0xFF)
#define COLOR_BLUE   RGB(0x33, 0x88, 0xFF)
#define COLOR_MAGENTA RGB(0xFF, 0x33, 0xFF)

void     fb_init(const fb_info_t *info);
uint32_t fb_width(void);
uint32_t fb_height(void);
uint32_t fb_pitch(void);
uint64_t fb_addr(void);
uint8_t  fb_bpp(void);

void     fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void     fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void     fb_clear(uint32_t color);
void     fb_scroll(uint32_t pixels, uint32_t bg);

#endif
