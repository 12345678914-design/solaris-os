#include "drivers/rtc.h"
#include "kernel/io.h"

static uint8_t cmos_leer(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int bcd_a_bin(uint8_t b) {
    return (b & 0x0F) + ((b >> 4) * 10);
}

static int actualizando(void) {
    return cmos_leer(0x0A) & 0x80;
}

void rtc_read(rtc_time_t *t) {
    while (actualizando()) { }

    uint8_t sec = cmos_leer(0x00);
    uint8_t min = cmos_leer(0x02);
    uint8_t hor = cmos_leer(0x04);
    uint8_t dia = cmos_leer(0x07);
    uint8_t mes = cmos_leer(0x08);
    uint8_t an  = cmos_leer(0x09);
    uint8_t regB = cmos_leer(0x0B);

    int pm = 0;
    if (!(regB & 0x04)) {              /* formato 12h */
        pm = hor & 0x80;
        hor &= 0x7F;
    }

    if (!(regB & 0x04)) {              /* BCD */
        sec = (uint8_t)bcd_a_bin(sec);
        min = (uint8_t)bcd_a_bin(min);
        hor = (uint8_t)bcd_a_bin(hor);
        dia = (uint8_t)bcd_a_bin(dia);
        mes = (uint8_t)bcd_a_bin(mes);
        an  = (uint8_t)bcd_a_bin(an);
    }

    if (pm && hor < 12) hor += 12;

    t->segundo = sec;
    t->minuto  = min;
    t->hora    = hor;
    t->dia     = dia;
    t->mes     = mes;
    t->anio    = 2000 + an;
}
