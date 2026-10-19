#ifndef VBE_H
#define VBE_H

#include <stdint.h>

/* Detecta la interfaz Bochs/QEMU VBE dispi. */
int vbe_bochs_available(void);

/* Programa un modo en el dispi de QEMU/Bochs. Actualiza los valores de
 * ancho y alto con el modo realmente aplicado (QEMU redondea el ancho
 * a un multiplo de 8). */
int vbe_bochs_set_mode(uint32_t *width, uint32_t *height, uint32_t bpp);

#endif
