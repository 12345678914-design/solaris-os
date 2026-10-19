#include <stdint.h>
#include "kernel/lib.h"
#include "kernel/io.h"
#include "kernel/heap.h"
#include "kernel/input.h"
#include "drivers/framebuffer.h"
#include "drivers/vbe.h"
#include "graphics/font.h"
#include "graphics/logo.h"
#include "terminal/terminal.h"
#include "fs/fsram.h"
#include "fs/ext4.h"
#include "gui/gui.h"

#define MAXCOMMAND 128
#define MB2_BOOTLOADER_MAGIC 0x36D76289

/* Resolucion objetivo en QEMU (no existe en la lista VBE estandar). */
#define WANT_WIDTH  1366
#define WANT_HEIGHT 768

static int g_entrar_gui = 0;

/* ------------------------- Encendido / reinicio --------------------------- */
void apagar(void) {
    outw(0x604, 0x2000);          /* QEMU */
    for (;;) __asm__ volatile ("cli; hlt");
}

void reiniciar(void) {
    __asm__ volatile ("cli");
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    for (;;) __asm__ volatile ("hlt");
}

/* ------------------------- MSR / MTRR ------------------------------------ */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t v) {
    __asm__ volatile ("wrmsr"
                      :: "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

static void mtrr_write_combining(uint64_t base, uint64_t size) {
    uint64_t cap = rdmsr(0xFE);
    if (!(cap & (1u << 8))) return;

    uint64_t pot = 1;
    while (pot < size) pot <<= 1;
    if (pot < 0x100000) pot = 0x100000;
    if (base & (pot - 1)) return;

    uint64_t def = rdmsr(0x2FF);
    wrmsr(0x2FF, def & ~0x800ULL);

    int slot = -1;
    for (int i = 0; i < 8; i++) {
        uint64_t mask = rdmsr(0x201 + i * 2);
        if (!(mask & 0x800)) { slot = i; break; }
    }
    if (slot < 0) { wrmsr(0x2FF, def); return; }

    wrmsr(0x200 + slot * 2, (base & ~0xFFFULL) | 1);
    wrmsr(0x201 + slot * 2,
          ((~pot + 1) & 0xFFFFFFFFFFF00000ULL) | 0x800ULL);

    wrmsr(0x2FF, def | 0x800ULL);
    __asm__ volatile ("wbinvd");
}

/* ------------------------- Multiboot2 -> framebuffer --------------------- */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint16_t reserved;
    uint8_t  r_pos, r_size;
    uint8_t  g_pos, g_size;
    uint8_t  b_pos, b_size;
} mb2_fb_tag_t;

static int mb2_get_framebuffer(uint32_t magic, uint64_t info, fb_info_t *out) {
    if (magic != MB2_BOOTLOADER_MAGIC) return 0;
    uint8_t *p   = (uint8_t *)(uintptr_t)info;
    uint32_t total = *(uint32_t *)p;
    uint8_t *end = p + total;
    p += 8;

    while (p + 8 <= end) {
        mb2_tag_t *tag = (mb2_tag_t *)p;
        if (tag->type == 0) break;
        if (tag->type == 8) {
            mb2_fb_tag_t *fb = (mb2_fb_tag_t *)p;
            out->addr   = fb->addr;
            out->width  = fb->width;
            out->height = fb->height;
            out->pitch  = fb->pitch;
            out->bpp    = fb->bpp;
            if (fb->fb_type == 1) {
                out->r_pos = fb->r_pos; out->r_size = fb->r_size;
                out->g_pos = fb->g_pos; out->g_size = fb->g_size;
                out->b_pos = fb->b_pos; out->b_size = fb->b_size;
            } else {
                out->r_pos = 16; out->r_size = 8;
                out->g_pos = 8;  out->g_size = 8;
                out->b_pos = 0;  out->b_size = 8;
            }
            return 1;
        }
        p += (tag->size + 7u) & ~7u;
    }
    return 0;
}

/* ------------------------- Utilidades de texto --------------------------- */
static uint32_t color_por_nombre(const char *color) {
    if (strcmp(color, "verde") == 0)    return COLOR_GREEN;
    if (strcmp(color, "amarillo") == 0) return COLOR_YELLOW;
    if (strcmp(color, "rosado") == 0)   return COLOR_PINK;
    if (strcmp(color, "rojo") == 0)     return COLOR_RED;
    if (strcmp(color, "cyan") == 0)     return COLOR_CYAN;
    if (strcmp(color, "gris") == 0)     return COLOR_GRAY;
    return COLOR_WHITE;
}

void imprimir(const char *text, const char *color, int salto) {
    term_set_color(color_por_nombre(color));
    term_write(text);
    if (salto) term_putchar('\n');
}

static void imprimir_u32(uint32_t n, const char *color, int salto) {
    char buf[12];
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
    } else {
        char t[12];
        int j = 0;
        while (n > 0) { t[j++] = (char)('0' + (n % 10)); n /= 10; }
        while (j > 0) buf[i++] = t[--j];
    }
    buf[i] = '\0';
    imprimir(buf, color, salto);
}

/* ------------------------- Entrada --------------------------------------- */
static char leer__tecla(void) {
    for (;;) {
        input_poll();
        int c = input_key();
        if (c) return (char)c;
        __asm__ volatile ("pause");
    }
}

static void leer_linea(char *out, int max) {
    int len = 0;
    out[0] = '\0';
    for (;;) {
        char t = leer__tecla();
        if (t == 0 || t == '\t' || t == KEY_ESC) continue;
        if (t == '\n') {
            out[len] = '\0';
            term_putchar('\n');
            return;
        }
        if (t == '\b') {
            if (len > 0) { len--; out[len] = '\0'; term_putchar('\b'); }
        } else if (len < max - 1) {
            out[len++] = t;
            out[len] = '\0';
            term_putchar(t);
        }
    }
}

/* ------------------------- Argumentos ------------------------------------ */
static char *next_arg(char **pp) {
    char *s = *pp;
    while (*s == ' ') s++;
    if (*s == '\0') { *pp = s; return 0; }
    char *start = s;
    while (*s && *s != ' ') s++;
    if (*s) { *s = '\0'; s++; }
    *pp = s;
    return start;
}

/* ------------------------- Comandos de archivos -------------------------- */
static void cmd_ls(void) {
    imprimir("", "blanco", 1);
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_file_t *f = fs_get(i);
        if (!f) continue;
        n++;
        term_set_color(COLOR_CYAN);
        term_write("  ");
        term_write(f->name);
        size_t len = strlen(f->name);
        for (size_t k = len; k < 22; k++) term_putchar(' ');
        imprimir_u32(f->size, "gris", 0);
        imprimir(" B", "gris", 1);
    }
    if (n == 0) imprimir("  (sin archivos)", "gris", 1);
    imprimir("", "blanco", 1);
}

static void cmd_create(const char *name) {
    if (!name) { imprimir("uso: create <archivo>", "rojo", 1); return; }
    int r = fs_create(name);
    if (r == FS_OK)               { imprimir("creado: ", "verde", 0); imprimir(name, "verde", 1); }
    else if (r == FS_ERR_EXISTS)  imprimir("ya existe", "rojo", 1);
    else if (r == FS_ERR_NAME)    imprimir("nombre invalido", "rojo", 1);
    else                          imprimir("sin espacio", "rojo", 1);
}

static void cmd_rm(const char *name) {
    if (!name) { imprimir("uso: rm <archivo>", "rojo", 1); return; }
    if (fs_delete(name) == FS_OK) { imprimir("eliminado: ", "verde", 0); imprimir(name, "verde", 1); }
    else                          { imprimir("no existe: ", "rojo", 0); imprimir(name, "rojo", 1); }
}

static void cmd_cat(const char *name) {
    if (!name) { imprimir("uso: cat <archivo>", "rojo", 1); return; }
    int i = fs_index_of(name);
    if (i < 0) { imprimir("no existe: ", "rojo", 0); imprimir(name, "rojo", 1); return; }
    fs_file_t *f = fs_get(i);
    imprimir("", "blanco", 1);
    term_set_color(COLOR_WHITE);
    for (uint32_t k = 0; k < f->size; k++) term_putchar((char)f->data[k]);
    if (f->size == 0 || f->data[f->size - 1] != '\n') term_putchar('\n');
    imprimir("", "blanco", 1);
}

static void cmd_write(const char *name, const char *content) {
    if (!name) { imprimir("uso: write <archivo> <texto>", "rojo", 1); return; }
    if (!content) content = "";
    if (fs_write(name, content, (uint32_t)strlen(content)) == FS_OK) {
        imprimir("escrito: ", "verde", 0);
        imprimir(name, "verde", 0);
        imprimir(" (", "verde", 0);
        imprimir_u32((uint32_t)strlen(content), "verde", 0);
        imprimir(" bytes)", "verde", 1);
    } else {
        imprimir("error al escribir", "rojo", 1);
    }
}

static void cmd_append(const char *name, const char *content) {
    if (!name) { imprimir("uso: append <archivo> <texto>", "rojo", 1); return; }
    if (!content) content = "";
    if (fs_append(name, content, (uint32_t)strlen(content)) != FS_OK ||
        fs_append(name, "\n", 1) != FS_OK) {
        imprimir("error al agregar", "rojo", 1);
        return;
    }
    imprimir("agregado a: ", "verde", 0);
    imprimir(name, "verde", 1);
}

static void cmd_edit(const char *name) {
    if (!name) { imprimir("uso: edit <archivo>", "rojo", 1); return; }
    if (!fs_exists(name)) {
        if (fs_create(name) != FS_OK) { imprimir("no se pudo crear", "rojo", 1); return; }
    }
    imprimir("", "blanco", 1);
    imprimir("Editor de lineas. Escribe :wq para guardar y salir.", "gris", 1);
    char linea[MAXCOMMAND];
    for (;;) {
        imprimir("| ", "cyan", 0);
        leer_linea(linea, MAXCOMMAND);
        if (strcmp(linea, ":wq") == 0 || strcmp(linea, ":q") == 0) break;
        fs_append(name, linea, (uint32_t)strlen(linea));
        fs_append(name, "\n", 1);
    }
    imprimir("guardado: ", "verde", 0);
    imprimir(name, "verde", 1);
}

static void cmd_mem(void) {
    imprimir("Heap del kernel:", "cyan", 1);
    imprimir("  usado ", "amarillo", 0);
    imprimir_u32((uint32_t)heap_used(), "amarillo", 0);
    imprimir(" / ", "amarillo", 0);
    imprimir_u32((uint32_t)heap_total(), "amarillo", 0);
    imprimir(" bytes", "amarillo", 1);
    imprimir("  archivos ", "amarillo", 0);
    imprimir_u32((uint32_t)fs_count(), "amarillo", 0);
    imprimir(" (", "amarillo", 0);
    imprimir_u32(fs_bytes_used(), "amarillo", 0);
    imprimir(" bytes)", "amarillo", 1);
}

/* ------------------------- Constructores de cadena ---------------------- */
static char *sb_cat(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

static char *sb_num(char *p, uint32_t n) {
    char t[12];
    int i = 0;
    if (n == 0) { *p++ = '0'; return p; }
    while (n > 0) { t[i++] = (char)('0' + (n % 10)); n /= 10; }
    while (i > 0) *p++ = t[--i];
    return p;
}

static char *sb_hex(char *p, uint64_t v) {
    static const char h[] = "0123456789ABCDEF";
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int d = (int)((v >> i) & 0xF);
        if (d || started || i == 0) { *p++ = h[d]; started = 1; }
    }
    return p;
}

/* ------------------------- Pantalla estilo fastfetch --------------------- */
static void pantalla_about(void) {
    term_clear();

    int      scale  = term_scale();
    uint32_t cw     = term_cell_w();
    uint32_t ch     = term_cell_h();
    int      sw     = (int)fb_width();
    int      sh     = (int)fb_height();

    int logo_w = logo_width_px(scale);
    int logo_h = logo_height_px(scale);

    int margen = 32;
    int logo_x = margen;
    int logo_y = (sh - logo_h) / 2;
    if (logo_y < margen) logo_y = margen;

    logo_draw((uint32_t)logo_x, (uint32_t)logo_y, COLOR_CYAN, scale);

    int info_x = logo_x + logo_w + (int)cw * 4;
    if (info_x > sw - (int)cw * 18) info_x = sw - (int)cw * 18;
    if (info_x < logo_x + logo_w + (int)cw) info_x = logo_x + logo_w + (int)cw;

    int y = logo_y + (int)ch;

    term_draw_text_at((uint32_t)info_x, (uint32_t)y, "isai@solaris", COLOR_GREEN);
    fb_fill_rect((uint32_t)info_x, (uint32_t)(y + (int)ch),
                 (uint32_t)(strlen("isai@solaris") * cw),
                 (ch >= 8 ? ch / 8 : 1), COLOR_GREEN);

    y += (int)ch * 2;

    char buf[96];
    char *p;

    term_draw_text_at((uint32_t)info_x, (uint32_t)y,
                      "OS:         Solaris 3.0 (x86_64 / long mode)", COLOR_WHITE);
    y += (int)ch;
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, "Kernel:     1.0", COLOR_WHITE);
    y += (int)ch;
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, "Host:       QEMU (Bochs VBE)", COLOR_WHITE);
    y += (int)ch;
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, "Shell:      solaris-sh", COLOR_WHITE);
    y += (int)ch;

    p = buf;
    p = sb_cat(p, "Heap:       ");
    p = sb_num(p, (uint32_t)heap_used());
    p = sb_cat(p, " / ");
    p = sb_num(p, (uint32_t)heap_total());
    *p = '\0';
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, buf, COLOR_WHITE);
    y += (int)ch;

    p = buf;
    p = sb_cat(p, "Resolucion: ");
    p = sb_num(p, fb_width());
    p = sb_cat(p, "x");
    p = sb_num(p, fb_height());
    p = sb_cat(p, "x");
    p = sb_num(p, fb_bpp());
    *p = '\0';
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, buf, COLOR_WHITE);
    y += (int)ch;

    p = buf;
    p = sb_cat(p, "Framebuffer: 0x");
    p = sb_hex(p, fb_addr());
    *p = '\0';
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, buf, COLOR_WHITE);
    y += (int)ch;

    p = buf;
    p = sb_cat(p, "Archivos:   ");
    p = sb_num(p, (uint32_t)fs_count());
    *p = '\0';
    term_draw_text_at((uint32_t)info_x, (uint32_t)y, buf, COLOR_WHITE);
    y += (int)ch;

    /* Barra de colores (estilo neofetch) */
    {
        static const uint32_t cols[8] = {
            COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN,
            COLOR_BLUE, COLOR_MAGENTA, COLOR_GRAY, COLOR_WHITE
        };
        int bw = (int)cw * 2;
        int by = y + (int)ch;
        for (int i = 0; i < 8; i++)
            fb_fill_rect((uint32_t)(info_x + i * bw), (uint32_t)by,
                         (uint32_t)bw, ch, cols[i]);
        y = by + (int)ch;
    }

    int abajo = logo_y + logo_h;
    if (y + (int)ch > abajo) abajo = y + (int)ch;
    term_set_cursor_px(0, (uint32_t)(abajo + (int)ch));
}

/* ------------------------- ext4 (disco principal) ------------------------ */
static void cmd_ext4(void) {
    if (!ext4_mounted()) {
        imprimir("ext4: no montado (sin disco o formato invalido)", "rojo", 1);
        return;
    }
    imprimir("ext4 (solo lectura) montado en /:", "verde", 1);
    ext4_dirent_t de;
    int n = 0;
    for (int i = 0; ; i++) {
        int r = ext4_readdir(ext4_root(), i, &de);
        if (r <= 0) break;
        if (de.name[0] == '.') continue;
        if (de.type == EXT4_FT_DIR) imprimir(de.name, "cyan", 1);
        else                        imprimir(de.name, "blanco", 1);
        n++;
    }
    if (n == 0) imprimir("(vacio)", "amarillo", 1);
}

/* ------------------------- Procesamiento de comandos --------------------- */
void procesar_comando(char *text) {
    char *p = text;
    char *cmd = next_arg(&p);

    if (!cmd || cmd[0] == '\0') { imprimir("", "blanco", 1); return; }

    if (strcmp(cmd, "help") == 0) {
        imprimir("", "blanco", 1);
        imprimir("Sistema:", "cyan", 1);
        imprimir("  help                    esta ayuda", "amarillo", 1);
        imprimir("  about                   informacion del sistema", "amarillo", 1);
        imprimir("  clear                   limpia la terminal", "amarillo", 1);
        imprimir("  gui                     inicia el escritorio grafico", "amarillo", 1);
        imprimir("  reboot / shutdown       reinicia / apaga", "amarillo", 1);
        imprimir("Archivos (fsram en RAM):", "cyan", 1);
        imprimir("  ls                      lista archivos", "amarillo", 1);
        imprimir("  create <n>              crea archivo", "amarillo", 1);
        imprimir("  write <n> <texto>       escribe (reemplaza)", "amarillo", 1);
        imprimir("  append <n> <texto>      agrega texto", "amarillo", 1);
        imprimir("  cat <n>                 muestra contenido", "amarillo", 1);
        imprimir("  rm <n>                  elimina archivo", "amarillo", 1);
        imprimir("  edit <n>                editor de lineas (:wq salir)", "amarillo", 1);
        imprimir("  mem                     uso de memoria", "amarillo", 1);
        imprimir("  ext4                    lista el disco ext4 (solo lectura)", "amarillo", 1);
    }
    else if (strcmp(cmd, "about") == 0)  pantalla_about();
    else if (strcmp(cmd, "clear") == 0)  term_clear();
    else if (strcmp(cmd, "gui") == 0)    g_entrar_gui = 1;
    else if (strcmp(cmd, "ls") == 0)     cmd_ls();
    else if (strcmp(cmd, "create") == 0) cmd_create(next_arg(&p));
    else if (strcmp(cmd, "rm") == 0)     cmd_rm(next_arg(&p));
    else if (strcmp(cmd, "cat") == 0)    cmd_cat(next_arg(&p));
    else if (strcmp(cmd, "write") == 0)  { char *n = next_arg(&p); cmd_write(n, p); }
    else if (strcmp(cmd, "append") == 0) { char *n = next_arg(&p); cmd_append(n, p); }
    else if (strcmp(cmd, "edit") == 0)   cmd_edit(next_arg(&p));
    else if (strcmp(cmd, "mem") == 0)    cmd_mem();
    else if (strcmp(cmd, "ext4") == 0)   cmd_ext4();
    else if (strcmp(cmd, "shutdown") == 0) { imprimir("Apagando...", "verde", 1); apagar(); }
    else if (strcmp(cmd, "reboot") == 0)   { imprimir("reiniciando...", "verde", 1); reiniciar(); }
    else {
        imprimir("comando desconocido '", "rojo", 0);
        imprimir(cmd, "rojo", 0);
        imprimir("' - escribe help", "rojo", 1);
    }
}

void leer_input(void) {
    char linea[MAXCOMMAND];
    for (;;) {
        leer_linea(linea, MAXCOMMAND);
        procesar_comando(linea);
        if (g_entrar_gui) { g_entrar_gui = 0; return; }
        imprimir("$solaris#ISAI> ", "amarillo", 0);
    }
}

/* ------------------------- Entrada al kernel ----------------------------- */
void kernel_main(uint32_t magic, uint64_t mb_info) {
    fb_info_t fbi;

    if (!mb2_get_framebuffer(magic, mb_info, &fbi)) {
        for (;;) __asm__ volatile ("cli; hlt");
    }

    if ((fbi.width != WANT_WIDTH || fbi.height != WANT_HEIGHT) &&
        vbe_bochs_available()) {
        uint32_t w = WANT_WIDTH, h = WANT_HEIGHT;
        if (vbe_bochs_set_mode(&w, &h, 32)) {
            fbi.width  = w;
            fbi.height = h;
            fbi.bpp    = 32;
            fbi.pitch  = w * 4;
            fbi.r_pos = 16; fbi.r_size = 8;
            fbi.g_pos = 8;  fbi.g_size = 8;
            fbi.b_pos = 0;  fbi.b_size = 8;
        }
    }

    fb_init(&fbi);
    mtrr_write_combining(fbi.addr, (uint64_t)fbi.pitch * fbi.height);
    heap_init();
    fs_init();
    ext4_init();
    term_init(COLOR_WHITE, COLOR_BLACK);
    input_init((int)fb_width(), (int)fb_height());

    imprimir("BIENVENIDO A SOLARIS 3.0", "verde", 1);
    imprimir("$solaris#ISAI> ", "amarillo", 0);

    for (;;) {
        leer_input();
        int rc = gui_run();
        term_init(COLOR_WHITE, COLOR_BLACK);
        if (rc < 0)
            imprimir("Error: no se pudo iniciar la GUI (sin memoria para el buffer)",
                     "rojo", 1);
        imprimir("BIENVENIDO A SOLARIS 3.0", "verde", 1);
        imprimir("$solaris#ISAI> ", "amarillo", 0);
    }
}
