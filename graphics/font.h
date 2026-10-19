#ifndef FONT_H_INCLUDED
#define FONT_H_INCLUDED

#include <stdint.h>

#define FONT_W 8
#define FONT_H 8

const uint8_t *font_glyph(char c);

/* Tamano de celda (en pixeles) para un porcentaje de escala (100 = 8x8). */
int  font_cell_w(int scale_pct);
int  font_cell_h(int scale_pct);

/* Dibuja un caracter con escala porcentual (100 = tamano base 8x8). */
void font_draw_char(uint32_t x, uint32_t y, char c,
                    uint32_t color, uint32_t bg, int scale_pct);

#endif
