#include "gui/wm.h"
#include "gui/gfx.h"
#include "kernel/input.h"
#include "kernel/lib.h"
#include "drivers/framebuffer.h"
#include "drivers/rtc.h"
#include "fs/fsram.h"

/* Acciones del sistema (definidas en kernel.c). */
void apagar(void);
void reiniciar(void);

#define WM_MAX     8
#define TB_H       44      /* alto de la barra de tareas */
#define TITLE_H    30      /* alto de la barra de titulo */
#define BTN        26      /* lado del boton cerrar */
#define UI_SCALE   2
#define TX_SCALE   1

/* ---- Paleta (0xRRGGBB) ---- */
#define C_DESK_TOP   0x1B3358
#define C_DESK_BOT   0x080C16
#define C_TASKBAR    0x14161C
#define C_TB_LINE    0x2A2F3A
#define C_BTN_FACE   0x2A303C
#define C_BTN_HOT    0x3A4250
#define C_ACCENT     0x2D6CDF
#define C_TITLE_OFF  0x343A46
#define C_WIN_BG     0x14181F
#define C_WIN_LINE   0x3A4150
#define C_TEXT       0xF0F2F5
#define C_TEXT_DIM   0xAAB2C0
#define C_GREEN      0x2FD37A
#define C_CLOSE      0xE0453A

typedef struct {
    int  used;
    int  x, y, w, h;
    char title[48];
    int  kind;
} win_t;

static win_t g_wins[WM_MAX];
static int   g_z[WM_MAX];    /* indices, de abajo hacia arriba */
static int   g_nz;
static int   g_focus;
static int   g_drag;         /* indice en arrastre (-1) */
static int   g_pmx, g_pmy;
static int   g_menu_open;

/* ------------------------------ Utilidades ------------------------------- */
static int scr_w(void) { return (int)fb_width(); }
static int scr_h(void) { return (int)fb_height(); }

static int u2a(uint32_t n, char *out) {
    char t[12];
    int i = 0, j = 0;
    if (n == 0) { out[0] = '0'; out[1] = '\0'; return 1; }
    while (n) { t[i++] = (char)('0' + n % 10); n /= 10; }
    while (i) out[j++] = t[--i];
    out[j] = '\0';
    return j;
}

static int str_w(const char *s, int scale) { return gfx_text_w(s, scale); }

/* ------------------------------ Z-order ---------------------------------- */
static void z_remove(int idx) {
    int j = 0;
    for (int i = 0; i < g_nz; i++)
        if (g_z[i] != idx) g_z[j++] = g_z[i];
    g_nz = j;
}

static void z_raise(int idx) {
    z_remove(idx);
    if (g_nz < WM_MAX) g_z[g_nz++] = idx;
}

static int z_top(void) { return g_nz ? g_z[g_nz - 1] : -1; }

/* ------------------------------ Ventanas --------------------------------- */
void wm_init(void) {
    for (int i = 0; i < WM_MAX; i++) g_wins[i].used = 0;
    g_nz = 0;
    g_focus = -1;
    g_drag = -1;
    g_menu_open = 0;
    g_pmx = input_mouse_x();
    g_pmy = input_mouse_y();
}

int wm_window_count(void) {
    int n = 0;
    for (int i = 0; i < WM_MAX; i++) if (g_wins[i].used) n++;
    return n;
}

void wm_open(const char *title, int x, int y, int w, int h, int kind) {
    int slot = -1;
    for (int i = 0; i < WM_MAX; i++) if (!g_wins[i].used) { slot = i; break; }
    if (slot < 0) return;

    win_t *wi = &g_wins[slot];
    wi->used = 1;
    wi->x = x; wi->y = y; wi->w = w; wi->h = h;
    wi->kind = kind;
    strncpy(wi->title, title, 47);
    wi->title[47] = '\0';

    z_raise(slot);
    g_focus = slot;
}

static void wm_close(int idx) {
    if (idx < 0 || !g_wins[idx].used) return;
    g_wins[idx].used = 0;
    z_remove(idx);
    if (g_focus == idx) g_focus = z_top();
    if (g_drag == idx) g_drag = -1;
}

static void clamp_win(win_t *w) {
    int maxx = scr_w() - 80;
    int minx = -w->w + 80;
    if (w->x > maxx) w->x = maxx;
    if (w->x < minx) w->x = minx;
    int maxy = scr_h() - TB_H - 20;
    if (w->y > maxy) w->y = maxy;
    if (w->y < 0) w->y = 0;
}

/* ------------------------------ Dibujo: ventana --------------------------- */
static void draw_close_btn(win_t *w, int activa) {
    int bx = w->x + w->w - BTN - 7;
    int by = w->y + (TITLE_H - BTN) / 2;
    gfx_fill(bx, by, BTN, BTN, C_CLOSE);
    /* X */
    uint32_t col = 0xFFFFFF;
    for (int i = 5; i < BTN - 5; i++) {
        gfx_pixel(bx + i, by + i, col);
        gfx_pixel(bx + i, by + (BTN - 1 - i), col);
        gfx_pixel(bx + i + 1, by + i, col);
        gfx_pixel(bx + i + 1, by + (BTN - 1 - i), col);
    }
    (void)activa;
}

static void draw_files(win_t *w) {
    int px = w->x + 12;
    int py = w->y + TITLE_H + 10;
    int scale = TX_SCALE;
    int lh = gfx_text_h(scale) + 6;

    gfx_text(px, py, "Sistema de archivos (RAM)", C_GREEN, 0xFFFFFFFF, scale);
    py += lh;
    gfx_fill(px, py, w->w - 24, 1, C_WIN_LINE);
    py += 6;

    int n = 0;
    uint32_t bytes = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_get(i);
        if (!f) continue;
        n++; bytes += f->size;

        gfx_text(px, py, f->name, C_TEXT, 0xFFFFFFFF, scale);

        char num[16];
        u2a(f->size, num);
        char buf[24];
        strcpy(buf, num);
        strcat(buf, " B");
        int numw = str_w(buf, scale);
        gfx_text(w->x + w->w - 12 - numw, py, buf, C_TEXT_DIM, 0xFFFFFFFF, scale);

        py += lh;
        if (py > w->y + w->h - 30) break;
    }
    if (n == 0)
        gfx_text(px, py, "(vacio)", C_TEXT_DIM, 0xFFFFFFFF, scale);

    /* pie */
    char buf[48];
    char num[16];
    u2a((uint32_t)n, num);
    strcpy(buf, num);
    strcat(buf, " archivos, ");
    u2a(bytes, num);
    strcat(buf, num);
    strcat(buf, " B");
    gfx_text(px, w->y + w->h - 24, buf, C_TEXT_DIM, 0xFFFFFFFF, scale);
}

static void draw_about(win_t *w) {
    int px = w->x + 12;
    int py = w->y + TITLE_H + 10;
    int scale = TX_SCALE;
    int lh = gfx_text_h(scale) + 6;

    gfx_text(px, py, "SOLARIS 3.0", C_GREEN, 0xFFFFFFFF, UI_SCALE); py += gfx_text_h(UI_SCALE) + 8;
    gfx_fill(px, py, w->w - 24, 1, C_WIN_LINE); py += 8;

    gfx_text(px, py, "Arquitectura: x86_64 / long mode", C_TEXT, 0xFFFFFFFF, scale); py += lh;
    gfx_text(px, py, "Kernel:       1.0", C_TEXT, 0xFFFFFFFF, scale); py += lh;
    gfx_text(px, py, "Shell:        solaris-sh", C_TEXT, 0xFFFFFFFF, scale); py += lh;
    gfx_text(px, py, "Filesystem:   fsram (en RAM)", C_TEXT, 0xFFFFFFFF, scale); py += lh;

    char buf[48];
    char num[16];
    strcpy(buf, "Resolucion:   ");
    u2a(fb_width(), num); strcat(buf, num); strcat(buf, "x");
    u2a(fb_height(), num); strcat(buf, num);
    gfx_text(px, py, buf, C_TEXT, 0xFFFFFFFF, scale); py += lh;

    strcpy(buf, "Memoria RAM:  512 MB");
    gfx_text(px, py, buf, C_TEXT, 0xFFFFFFFF, scale); py += lh;

    gfx_text(px, py, "Graficos:     doble buffer + MTRR WC", C_TEXT, 0xFFFFFFFF, scale);
}

static void draw_window(int idx) {
    win_t *w = &g_wins[idx];
    int activa = (idx == g_focus);

    gfx_clip_set(w->x - 6, w->y - 6, w->w + 14, w->h + 16);

    /* sombra */
    gfx_fill(w->x + 6, w->y + 6, w->w, w->h, 0x05070C);

    /* barra de titulo */
    uint32_t tcol = activa ? C_ACCENT : C_TITLE_OFF;
    gfx_fill(w->x, w->y, w->w, TITLE_H, tcol);
    gfx_text(w->x + 12, w->y + (TITLE_H - gfx_text_h(UI_SCALE)) / 2,
             w->title, C_TEXT, 0xFFFFFFFF, UI_SCALE);
    draw_close_btn(w, activa);

    /* cuerpo */
    gfx_fill(w->x, w->y + TITLE_H, w->w, w->h - TITLE_H, C_WIN_BG);
    gfx_frame(w->x, w->y, w->w, w->h, C_WIN_LINE);

    /* contenido */
    gfx_clip_set(w->x, w->y + TITLE_H, w->w, w->h - TITLE_H);
    if (w->kind == 0)      draw_files(w);
    else if (w->kind == 1) draw_about(w);
    gfx_clip_reset();
}

/* ------------------------------ Dibujo: barra ----------------------------- */
static int tb_window_rect(int k, int *bx, int *bw) {
    int x = 128;
    for (int i = 0; i < g_nz; i++) {
        int idx = g_z[i];
        int tw = str_w(g_wins[idx].title, 1) + 26;
        if (i == k) { *bx = x; *bw = tw; return idx; }
        x += tw + 6;
    }
    *bx = x; *bw = 0;
    return -1;
}

static void draw_taskbar(void) {
    int sw = scr_w(), sh = scr_h();
    int ty = sh - TB_H;

    gfx_fill(0, ty, sw, TB_H, C_TASKBAR);
    gfx_fill(0, ty, sw, 2, C_TB_LINE);

    /* Boton inicio */
    gfx_fill(8, ty + 7, 112, TB_H - 14, C_BTN_FACE);
    gfx_frame(8, ty + 7, 112, TB_H - 14, C_ACCENT);
    gfx_text(24, ty + (TB_H - gfx_text_h(UI_SCALE)) / 2,
             "SOLARIS", C_TEXT, 0xFFFFFFFF, UI_SCALE);

    /* Botones de ventana */
    for (int k = 0; k < g_nz; k++) {
        int bx, bw;
        int idx = tb_window_rect(k, &bx, &bw);
        int activa = (idx == g_focus);
        gfx_fill(bx, ty + 7, bw, TB_H - 14, activa ? C_ACCENT : C_BTN_FACE);
        gfx_text(bx + 12, ty + (TB_H - gfx_text_h(1)) / 2,
                 g_wins[idx].title, C_TEXT, 0xFFFFFFFF, 1);
    }

    /* Reloj */
    rtc_time_t t;
    rtc_read(&t);
    char buf[32];
    char num[8];
    buf[0] = '\0';
    u2a((uint32_t)t.hora, num); if (t.hora < 10) strcat(buf, "0"); strcat(buf, num); strcat(buf, ":");
    u2a((uint32_t)t.minuto, num); if (t.minuto < 10) strcat(buf, "0"); strcat(buf, num); strcat(buf, ":");
    u2a((uint32_t)t.segundo, num); if (t.segundo < 10) strcat(buf, "0"); strcat(buf, num);

    int cw = str_w(buf, UI_SCALE);
    gfx_text(sw - cw - 16, ty + (TB_H - gfx_text_h(UI_SCALE)) / 2,
             buf, C_TEXT, 0xFFFFFFFF, UI_SCALE);
}

/* ------------------------------ Menu inicio ------------------------------- */
typedef struct { const char *label; int kind; } menu_item_t;
static const menu_item_t g_menu[] = {
    { "Archivos",   0 },
    { "Acerca de",  1 },
    { "Reiniciar", -1 },
    { "Apagar",    -2 },
};
#define MENU_N ((int)(sizeof(g_menu)/sizeof(g_menu[0])))
#define MENU_W 220
#define MENU_IH 36

static int menu_rect(int *mx, int *my, int *mw, int *mh) {
    int sh = scr_h();
    *mh = MENU_N * MENU_IH + 8;
    *mw = MENU_W;
    *mx = 8;
    *my = sh - TB_H - *mh - 6;
    return 1;
}

static void draw_menu(void) {
    if (!g_menu_open) return;
    int mx, my, mw, mh;
    menu_rect(&mx, &my, &mw, &mh);

    gfx_fill(mx + 4, my + 4, mw, mh, 0x05070C);
    gfx_fill(mx, my, mw, mh, 0x1E232C);
    gfx_frame(mx, my, mw, mh, C_ACCENT);

    for (int i = 0; i < MENU_N; i++) {
        int iy = my + 4 + i * MENU_IH;
        gfx_text(mx + 16, iy + (MENU_IH - gfx_text_h(1)) / 2,
                 g_menu[i].label, C_TEXT, 0xFFFFFFFF, 1);
    }
}

static void menu_click(int myy) {
    int mx, my, mw, mh;
    menu_rect(&mx, &my, &mw, &mh);
    (void)mx; (void)mw; (void)mh;
    int i = (myy - my - 4) / MENU_IH;
    if (i < 0 || i >= MENU_N) return;
    int kind = g_menu[i].kind;
    g_menu_open = 0;
    if (kind == -1) { reiniciar(); }
    else if (kind == -2) { apagar(); }
    else if (kind == 0) { wm_open("Archivos", 140, 110, 520, 380, 0); }
    else if (kind == 1) { wm_open("Acerca de", 560, 160, 420, 300, 1); }
}

/* ------------------------------ Escritorio -------------------------------- */
static void draw_desktop(void) {
    gfx_vgradient(0, 0, scr_w(), scr_h(), C_DESK_TOP, C_DESK_BOT);
}

/* ------------------------------ Cursor ------------------------------------ */
static const uint16_t g_cursor[16] = {
    0x001, 0x003, 0x005, 0x009, 0x011, 0x021, 0x041, 0x081,
    0x101, 0x201, 0x7C1, 0x079, 0x025, 0x023, 0x041, 0x000
};

static void draw_cursor(void) {
    int mx = input_mouse_x();
    int my = input_mouse_y();
    /* contorno negro (8 vecinos) */
    static const int ox[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int oy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 12; c++)
            if (g_cursor[r] & (1u << c))
                for (int k = 0; k < 8; k++)
                    gfx_pixel(mx + c + ox[k], my + r + oy[k], 0x000000);
    /* relleno blanco */
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 12; c++)
            if (g_cursor[r] & (1u << c))
                gfx_pixel(mx + c, my + r, 0xFFFFFF);
}

/* ------------------------------ Entrada ----------------------------------- */
static int hit_window(int x, int y) {
    for (int i = g_nz - 1; i >= 0; i--) {
        int idx = g_z[i];
        win_t *w = &g_wins[idx];
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return idx;
    }
    return -1;
}

int wm_update(void) {
    int mx = input_mouse_x(), my = input_mouse_y();
    int left = input_mouse_left();
    int moved = input_mouse_take_moved();
    int click = input_mouse_take_click();
    int dx = mx - g_pmx, dy = my - g_pmy;
    g_pmx = mx; g_pmy = my;

    int changed = moved || click;

    if (g_drag >= 0) {
        if (left) {
            g_wins[g_drag].x += dx;
            g_wins[g_drag].y += dy;
            clamp_win(&g_wins[g_drag]);
            changed = 1;
        } else {
            g_drag = -1;
        }
    }

    if (click) {
        int sh = scr_h();
        int ty = sh - TB_H;

        /* Menu abierto: tiene prioridad. */
        if (g_menu_open) {
            int mmx, mmy, mmw, mmh;
            menu_rect(&mmx, &mmy, &mmw, &mmh);
            if (mx >= mmx && mx < mmx + mmw && my >= mmy && my < mmy + mmh) {
                menu_click(my);
                return 1;
            }
            g_menu_open = 0;
        }

        /* Barra de tareas */
        if (my >= ty) {
            if (mx >= 8 && mx < 120 && my >= ty + 7 && my < ty + TB_H - 7) {
                g_menu_open = !g_menu_open;
                return 1;
            }
            for (int k = 0; k < g_nz; k++) {
                int bx, bw;
                int idx = tb_window_rect(k, &bx, &bw);
                if (mx >= bx && mx < bx + bw && my >= ty + 7 && my < ty + TB_H - 7) {
                    z_raise(idx);
                    g_focus = idx;
                    return 1;
                }
            }
            return 1;
        }

        /* Ventanas */
        int hit = hit_window(mx, my);
        if (hit >= 0) {
            win_t *w = &g_wins[hit];
            int cbx = w->x + w->w - BTN - 7;
            int cby = w->y + (TITLE_H - BTN) / 2;
            if (mx >= cbx && mx < cbx + BTN && my >= cby && my < cby + BTN) {
                wm_close(hit);
                return 1;
            }
            z_raise(hit);
            g_focus = hit;
            if (my < w->y + TITLE_H) g_drag = hit;
        }
        return 1;
    }

    return changed;
}

/* ------------------------------ Composicion ------------------------------- */
void wm_draw(void) {
    gfx_clip_reset();
    draw_desktop();

    for (int i = 0; i < g_nz; i++) draw_window(g_z[i]);

    draw_taskbar();
    draw_menu();

    gfx_clip_reset();
    draw_cursor();
}
