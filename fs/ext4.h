#ifndef EXT4_H
#define EXT4_H

#include <stdint.h>

#define EXT4_FT_REG  1
#define EXT4_FT_DIR  2
#define EXT4_FT_LNK  7

typedef struct {
    uint32_t ino;
    uint8_t  type;      /* EXT4_FT_* */
    char     name[256];
} ext4_dirent_t;

int  ext4_init(void);                 /* 0 = montado OK */
int  ext4_mounted(void);
uint32_t ext4_root(void);             /* inodo de la raiz (2) */

/* Iterador de directorio: index=0..N-1. Devuelve 1 si hay entrada,
   0 al terminar, -1 en error. */
int  ext4_readdir(uint32_t dir, int index, ext4_dirent_t *out);

/* Lee el contenido de un archivo (hasta `max` bytes). Devuelve leidos o -1. */
int  ext4_read(uint32_t ino, uint8_t *buf, uint32_t max);

int  ext4_isdir(uint32_t ino);
uint32_t ext4_size(uint32_t ino);

#endif
