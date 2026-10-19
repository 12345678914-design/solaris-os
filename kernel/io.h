#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline void outb(uint16_t puerto, uint8_t valor) {
    __asm__ volatile ("outb %0, %1" :: "a"(valor), "Nd"(puerto));
}

static inline void outw(uint16_t puerto, uint16_t valor) {
    __asm__ volatile ("outw %0, %1" :: "a"(valor), "Nd"(puerto));
}

static inline uint8_t inb(uint16_t puerto) {
    uint8_t resultado;
    __asm__ volatile ("inb %w1, %0" : "=a"(resultado) : "Nd"(puerto));
    return resultado;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
