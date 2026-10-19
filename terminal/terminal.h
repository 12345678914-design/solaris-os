#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

/* Escala porcentual de la fuente de la terminal (100 = 8x8). */
#define TERM_SCALE 125

void     term_init(uint32_t fg, uint32_t bg);
void     term_clear(void);
void     term_set_color(uint32_t fg);
void     term_set_bg(uint32_t bg);
void     term_putchar(char c);
void     term_write(const char *s);
void     term_newline(void);
uint32_t term_color(void);
uint32_t term_bg(void);
uint32_t term_cell_w(void);
uint32_t term_cell_h(void);
int      term_scale(void);

/* Dibuja texto en una posicion absoluta en pixeles (no mueve el cursor). */
void     term_draw_text_at(uint32_t x, uint32_t y, const char *s, uint32_t color);

/* Coloca el cursor de la terminal en una posicion en pixeles. */
void     term_set_cursor_px(uint32_t x, uint32_t y);

#endif
