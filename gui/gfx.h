#ifndef GFX_H
#define GFX_H

#include <stdint.h>

/* Capa grafica con doble buffer (se dibuja en RAM y luego se presenta). */
void gfx_init(void);
int  gfx_ok(void);

void gfx_clip_set(int x, int y, int w, int h);
void gfx_clip_reset(void);

void gfx_clear(uint32_t color);
void gfx_pixel(int x, int y, uint32_t color);
void gfx_fill(int x, int y, int w, int h, uint32_t color);
void gfx_frame(int x, int y, int w, int h, uint32_t color);
void gfx_vgradient(int x, int y, int w, int h, uint32_t arriba, uint32_t abajo);

void gfx_char(int x, int y, char c, uint32_t fg, uint32_t bg, int scale);
void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale);
int  gfx_text_w(const char *s, int scale);
int  gfx_text_h(int scale);

void gfx_present(void);   /* vuelca el buffer al framebuffer real */

uint32_t gfx_map_rgb(uint32_t rgb);   /* color 0xRRGGBB -> formato del buffer */

#endif
