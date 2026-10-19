#ifndef LOGO_H
#define LOGO_H

#include <stdint.h>

int logo_cols(void);
int logo_rows(void);
int logo_width_px(int scale_pct);
int logo_height_px(int scale_pct);

/* Dibuja el logo TON 618 en (x,y) en pixeles, con escala porcentual. */
void logo_draw(uint32_t x, uint32_t y, uint32_t color, int scale_pct);

#endif
