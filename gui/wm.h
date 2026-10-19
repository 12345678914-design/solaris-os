#ifndef WM_H
#define WM_H

#include <stdint.h>

/* Gestor de ventanas sencillo: escritorio, barra de tareas y ventanas. */

void wm_init(void);
int  wm_open(const char *title, int x, int y, int w, int h, int kind);
void wm_set_dir(int slot, uint32_t ino);
int  wm_update(void);   /* procesa raton/teclado; 1 si hubo cambios */
void wm_draw(void);     /* compone el cuadro en el buffer de gfx */

int  wm_window_count(void);

#endif
