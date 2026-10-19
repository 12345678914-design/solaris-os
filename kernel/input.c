#include "kernel/input.h"
#include "kernel/io.h"
#include "drivers/mouse.h"

#define KEYQ_SIZE 64

static char   g_keyq[KEYQ_SIZE];
static int    g_head, g_tail;

static const char mapa_teclas[128] = {
    [0x01] = 27,  /* ESC */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x39] = ' ',
};

static void push_key(char c) {
    int t = (g_tail + 1) % KEYQ_SIZE;
    if (t == g_head) return;    /* cola llena */
    g_keyq[g_tail] = c;
    g_tail = t;
}

void input_init(int screen_w, int screen_h) {
    g_head = g_tail = 0;
    mouse_set_bounds(screen_w, screen_h);
    mouse_init();
}

void input_poll(void) {
    for (;;) {
        /* Leer el estado UNA sola vez: evita desincronizar el origen
           (raton/teclado) cuando hay datos de ambos en el buffer. */
        uint8_t st = inb(0x64);
        if (!(st & 0x01)) break;
        uint8_t d = inb(0x60);

        if (st & 0x20) {            /* viene del raton */
            mouse_feed(d);
        } else {                    /* viene del teclado */
            if (d & 0x80) continue; /* tecla liberada */
            char c = mapa_teclas[d];
            if (c) push_key(c);
        }
    }
}

void input_flush(void) {
    input_poll();
    g_head = g_tail = 0;
}

int input_key(void) {
    if (g_head == g_tail) return 0;
    char c = g_keyq[g_head];
    g_head = (g_head + 1) % KEYQ_SIZE;
    return (int)(unsigned char)c;
}

int input_mouse_x(void)           { return mouse_x(); }
int input_mouse_y(void)           { return mouse_y(); }
int input_mouse_left(void)        { return mouse_left(); }
int input_mouse_right(void)       { return mouse_right(); }
int input_mouse_take_moved(void)  { return mouse_take_moved(); }
int input_mouse_take_click(void)  { return mouse_take_click(); }
