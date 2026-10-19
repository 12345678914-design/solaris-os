#include "gui/gui.h"
#include "gui/gfx.h"
#include "gui/wm.h"
#include "kernel/input.h"
#include "drivers/rtc.h"
#include "fs/ext4.h"

int gui_run(void) {
    gfx_init();
    if (!gfx_ok()) return -1;

    wm_init();
    int f = wm_open("Archivos", 120, 90, 560, 400, 0);
    if (f >= 0) wm_set_dir(f, ext4_root());
    wm_open("Acerca de", 620, 180, 420, 300, 1);

    /* Descartar bytes residuales de teclado/raton que llegaron durante el
       cambio de modo y la reserva del buffer, para que ninguno se interprete
       como ESC y cierre la GUI al instante. */
    for (int i = 0; i < 4; i++) {
        for (volatile uint32_t d = 0; d < 3000000u; d++)
            __asm__ volatile ("pause");
        input_flush();
    }

    int dirty = 1;
    int last_sec = -1;

    for (;;) {
        input_poll();

        int key = input_key();
        if (key == KEY_ESC) break;

        int changed = wm_update();

        rtc_time_t t;
        rtc_read(&t);
        if (t.segundo != last_sec) {
            last_sec = t.segundo;
            changed = 1;
        }

        if (changed || key) dirty = 1;

        if (dirty) {
            wm_draw();
            gfx_present();
            dirty = 0;
        }
    }
    return 0;
}
