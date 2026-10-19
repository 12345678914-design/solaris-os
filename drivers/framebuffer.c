#include "drivers/framebuffer.h"
#include "kernel/lib.h"

static volatile uint8_t *g_fb    = 0;
static uint32_t          g_w     = 0;
static uint32_t          g_h     = 0;
static uint32_t          g_pitch = 0;
static uint8_t           g_bpp   = 32;
static uint8_t           g_rp, g_rs, g_gp, g_gs, g_bp, g_bs;

void fb_init(const fb_info_t *info) {
    g_fb    = (volatile uint8_t *)(uintptr_t)info->addr;
    g_w     = info->width;
    g_h     = info->height;
    g_pitch = info->pitch;
    g_bpp   = info->bpp;
    g_rp = info->r_pos; g_rs = info->r_size;
    g_gp = info->g_pos; g_gs = info->g_size;
    g_bp = info->b_pos; g_bs = info->b_size;
}

uint32_t fb_width(void)  { return g_w; }
uint32_t fb_height(void) { return g_h; }
uint32_t fb_pitch(void)  { return g_pitch; }
uint64_t fb_addr(void)   { return (uint64_t)(uintptr_t)g_fb; }
uint8_t  fb_bpp(void)    { return g_bpp; }

/* Escala un canal de 8 bits a un campo de 'size' bits en la posicion 'pos'. */
static inline uint32_t scale_channel(uint8_t v, uint8_t size, uint8_t pos) {
    if (size == 0) return 0;
    uint32_t max = (1u << size) - 1u;
    uint32_t val = ((uint32_t)v * max) / 255u;
    return val << pos;
}

/* Empaqueta 0xRRGGBB al formato nativo del framebuffer. */
static inline uint32_t pack(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b = (color)       & 0xFF;
    return scale_channel(r, g_rs, g_rp)
         | scale_channel(g, g_gs, g_gp)
         | scale_channel(b, g_bs, g_bp);
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_w || y >= g_h) return;
    uint8_t *dst = (uint8_t *)g_fb + (uint64_t)y * g_pitch;

    if (g_bpp == 32) {
        *(volatile uint32_t *)(dst + x * 4) = pack(color);
    } else if (g_bpp == 24) {
        uint32_t c = pack(color);
        dst[x * 3 + 0] = (uint8_t)(c & 0xFF);
        dst[x * 3 + 1] = (uint8_t)((c >> 8) & 0xFF);
        dst[x * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
    } else if (g_bpp == 16) {
        *(volatile uint16_t *)(dst + x * 2) = (uint16_t)pack(color);
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (w == 0 || h == 0) return;
    if (x >= g_w || y >= g_h) return;
    if (x + w > g_w) w = g_w - x;
    if (y + h > g_h) h = g_h - y;

    uint32_t packed = pack(color);

    if (g_bpp == 32) {
        for (uint32_t j = 0; j < h; j++) {
            uint32_t *row = (uint32_t *)((uint8_t *)g_fb
                           + (uint64_t)(y + j) * g_pitch) + x;
            for (uint32_t i = 0; i < w; i++) row[i] = packed;
        }
    } else if (g_bpp == 24) {
        for (uint32_t j = 0; j < h; j++) {
            uint8_t *row = (uint8_t *)g_fb + (uint64_t)(y + j) * g_pitch + x * 3;
            for (uint32_t i = 0; i < w; i++) {
                row[i * 3 + 0] = (uint8_t)(packed & 0xFF);
                row[i * 3 + 1] = (uint8_t)((packed >> 8) & 0xFF);
                row[i * 3 + 2] = (uint8_t)((packed >> 16) & 0xFF);
            }
        }
    } else if (g_bpp == 16) {
        for (uint32_t j = 0; j < h; j++) {
            uint16_t *row = (uint16_t *)((uint8_t *)g_fb
                           + (uint64_t)(y + j) * g_pitch) + x;
            for (uint32_t i = 0; i < w; i++) row[i] = (uint16_t)packed;
        }
    }
}

void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, g_w, g_h, color);
}

void fb_scroll(uint32_t pixels, uint32_t bg) {
    if (pixels == 0) return;
    if (pixels >= g_h) {
        fb_clear(bg);
        return;
    }
    uint8_t *base = (uint8_t *)g_fb;
    uint32_t move = (g_h - pixels) * g_pitch;
    memmove(base, base + (uint64_t)pixels * g_pitch, move);

    /* Rellenar las filas nuevas de abajo */
    fb_fill_rect(0, g_h - pixels, g_w, pixels, bg);
}
