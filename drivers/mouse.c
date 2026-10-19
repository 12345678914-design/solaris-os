#include "drivers/mouse.h"
#include "kernel/io.h"

static int g_x, g_y;
static int g_w = 1024, g_h = 768;
static int g_izq, g_der;
static int g_movido, g_click;

/* Tamano del puntero: el cursor no debe salirse por abajo/derecha. */
#define CURSOR_W 12
#define CURSOR_H 16
#define CURSOR_MARGIN 2

static void mouse_clamp(void) {
    int maxx = g_w - CURSOR_W - CURSOR_MARGIN;
    int maxy = g_h - CURSOR_H - CURSOR_MARGIN;
    if (maxx < 0) maxx = 0;
    if (maxy < 0) maxy = 0;
    if (g_x < 0) g_x = 0;
    if (g_y < 0) g_y = 0;
    if (g_x > maxx) g_x = maxx;
    if (g_y > maxy) g_y = maxy;
}

static uint8_t g_pkt[3];
static int     g_idx;

/* --------------------- Acceso al controlador PS/2 ------------------------ */
static int esperar_entrada_libre(void) {
    for (int t = 0; t < 200000; t++)
        if (!(inb(0x64) & 0x02)) return 1;
    return 0;
}

static int esperar_salida(void) {
    for (int t = 0; t < 200000; t++)
        if (inb(0x64) & 0x01) return 1;
    return 0;
}

static void mouse_cmd(uint8_t valor) {
    esperar_entrada_libre();
    outb(0x64, 0xD4);           /* siguiente byte va al raton */
    esperar_entrada_libre();
    outb(0x60, valor);
}

static uint8_t mouse_resp(void) {
    if (!esperar_salida()) return 0;
    return inb(0x60);
}

void mouse_set_bounds(int w, int h) {
    g_w = w; g_h = h;
    mouse_clamp();
}

void mouse_init(void) {
    g_x = g_w / 2;
    g_y = g_h / 2;
    g_izq = g_der = 0;
    g_movido = g_click = 0;
    g_idx = 0;

    esperar_entrada_libre();
    outb(0x64, 0xA8);           /* habilitar dispositivo auxiliar */

    esperar_entrada_libre();
    outb(0x64, 0x20);           /* leer byte de estado (compaq) */
    uint8_t st = mouse_resp();
    st |= 0x02;                 /* IRQ12 (inofensivo, IF=0) */
    st &= (uint8_t)~0x20;       /* habilitar reloj del raton */
    esperar_entrada_libre();
    outb(0x64, 0x60);
    esperar_entrada_libre();
    outb(0x60, st);

    mouse_cmd(0xF6); mouse_resp();   /* valores por defecto */
    mouse_cmd(0xF4); mouse_resp();   /* habilitar envio de datos */

    /* Vaciar lo pendiente */
    for (int t = 0; t < 100 && (inb(0x64) & 0x01); t++) inb(0x60);
}

void mouse_feed(uint8_t byte) {
    /* Resincronizar si el primer byte no tiene el bit 3 fijo en 1. */
    if (g_idx == 0 && !(byte & 0x08)) return;

    g_pkt[g_idx++] = byte;
    if (g_idx < 3) return;
    g_idx = 0;

    uint8_t fl = g_pkt[0];
    int dx = g_pkt[1];
    int dy = g_pkt[2];
    if (fl & 0x10) dx -= 256;   /* signo X */
    if (fl & 0x20) dy -= 256;   /* signo Y (positivo = arriba) */

    /* Bits de desbordamiento (bit6 = X, bit7 = Y): el movimiento fue mayor
       de lo que cabe en 8 bits. Saturar en vez de usar un delta corrupto,
       que hacia saltar el cursor a una esquina. */
    if (fl & 0x40) dx = (fl & 0x10) ? -128 : 127;
    if (fl & 0x80) dy = (fl & 0x20) ? -128 : 127;

    g_x += dx;
    g_y -= dy;
    mouse_clamp();

    int nl = fl & 0x01;
    int nr = (fl >> 1) & 0x01;
    if (nl && !g_izq) g_click = 1;
    g_izq = nl;
    g_der = nr;
    g_movido = 1;
}

int mouse_x(void)              { return g_x; }
int mouse_y(void)              { return g_y; }
int mouse_left(void)           { return g_izq; }
int mouse_right(void)          { return g_der; }
int mouse_take_moved(void)     { int v = g_movido; g_movido = 0; return v; }
int mouse_take_click(void)     { int v = g_click;  g_click  = 0; return v; }
