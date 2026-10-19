#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    int hora, minuto, segundo;
    int dia, mes, anio;
} rtc_time_t;

void rtc_read(rtc_time_t *t);

#endif
