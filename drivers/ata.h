#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Lee `count` sectores (512 bytes c/u) desde LBA (28 bits) a `buf`.
   Devuelve 0 si OK, -1 si no hay disco o hay error. */
int ata_read_sectors(uint32_t lba, uint8_t count, void *buf);

/* Detecta el disco primario maestro. 1 si esta presente. */
int ata_present(void);

#endif
