#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

void input_init(int screen_w, int screen_h);
void input_poll(void);           /* drena el controlador PS/2 */
void input_flush(void);          /* drena controlador y cola de teclas */

int  input_key(void);            /* devuelve tecla (0 si no hay) */

int  input_mouse_x(void);
int  input_mouse_y(void);
int  input_mouse_left(void);
int  input_mouse_right(void);
int  input_mouse_take_moved(void);
int  input_mouse_take_click(void);

#define KEY_ESC 27

#endif
