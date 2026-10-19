#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

/* Raton PS/2 en modo poll (sin IRQ). */
void mouse_init(void);
void mouse_set_bounds(int w, int h);
void mouse_feed(uint8_t byte);      /* alimenta un byte del puerto 0x60 */

int  mouse_x(void);
int  mouse_y(void);
int  mouse_left(void);
int  mouse_right(void);
int  mouse_take_moved(void);        /* consume bandera de movimiento */
int  mouse_take_click(void);        /* consume bandera de click izquierdo */

#endif
