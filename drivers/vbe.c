#include "drivers/vbe.h"

/*
 * Interfaz Bochs VBE dispi usada por QEMU (-vga std / dispositivo VGA).
 * Permite fijar resoluciones arbitrarias como 1366x768 que no existen
 * en la lista de modos VBE estandar.
 */

#define DISPI_PORT_INDEX 0x1CE
#define DISPI_PORT_DATA  0x1CF

#define DISPI_INDEX_ID         0
#define DISPI_INDEX_XRES       1
#define DISPI_INDEX_YRES       2
#define DISPI_INDEX_BPP        3
#define DISPI_INDEX_ENABLE     4
#define DISPI_INDEX_VIRT_WIDTH 6

#define DISPI_DISABLED      0x00
#define DISPI_ENABLED       0x01
#define DISPI_LFB_ENABLED   0x40

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t r;
    __asm__ volatile ("inw %w1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static void dispi_write(uint16_t index, uint16_t value) {
    outw(DISPI_PORT_INDEX, index);
    outw(DISPI_PORT_DATA, value);
}

static uint16_t dispi_read(uint16_t index) {
    outw(DISPI_PORT_INDEX, index);
    return inw(DISPI_PORT_DATA);
}

int vbe_bochs_available(void) {
    uint16_t id = dispi_read(DISPI_INDEX_ID);
    /* VBE_DISPI_ID0..ID5 = 0xB0C0 .. 0xB0C5 */
    return (id >= 0xB0C0 && id <= 0xB0C6);
}

int vbe_bochs_set_mode(uint32_t *width, uint32_t *height, uint32_t bpp) {
    if (!vbe_bochs_available()) return 0;

    dispi_write(DISPI_INDEX_ENABLE, DISPI_DISABLED);
    dispi_write(DISPI_INDEX_XRES, (uint16_t)*width);
    dispi_write(DISPI_INDEX_YRES, (uint16_t)*height);
    dispi_write(DISPI_INDEX_BPP, (uint16_t)bpp);
    dispi_write(DISPI_INDEX_VIRT_WIDTH, (uint16_t)*width);
    dispi_write(DISPI_INDEX_ENABLE, DISPI_ENABLED | DISPI_LFB_ENABLED);

    /* Leer el modo realmente aplicado por el hardware. */
    *width  = dispi_read(DISPI_INDEX_XRES);
    *height = dispi_read(DISPI_INDEX_YRES);
    return (*width != 0 && *height != 0);
}
