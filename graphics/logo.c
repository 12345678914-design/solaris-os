#include "graphics/logo.h"
#include "graphics/font.h"
#include "drivers/framebuffer.h"

/* Arte ASCII de TON 618 (agujero negro supermasivo). */
static const char *logo_arte[] = {
"                                        .                                       ",
"                                        : .                                     ",
"                                      ..-                                       ",
"                                       :=.  .                                   ",
"                                       :*:  .                                   ",
"                                    :..-*:...                                   ",
"                               ....:=--+#=-:=-...                               ",
"                             ::--++**#**##+**#==-:...                ...        ",
"                          ...:-:=#+--:.   .:+*+=--:::..........::::::::::-:...  ",
"                         ..::+*+-::=+##*##**+-.-*+*+-:--:::---==+++==--:.--:.   ",
"                        .::-=+-.-#*-.      .-#+=:-++++*##**###***=-:==-:-:..   .",
"                       ..:=+=:-=*-            .*=:.:-+###*+==+##*=----::..     .",
"                      .:-++=.:++.              .*+:  :=######+=-:--::..        .",
"                    ..:=*==:.=#-                =#*######+=-=---:.....         .",
"                ...::-==++:. =#.           ..+*###*+=-=+-:::....               .",
"             ...:::=+==*+:.  -*:     ..-*#*##*==+=---:.:...                    .",
"          .::-:-=++++**==:  .-*#+**###*+*+=+==:-*=:......                      .",
"       .:::----*#+-*###########*+*==+=-:-: .  :*+:......                       .",
"     ::::==:=++*#*=++++**+++--=--+++-.     -+#+=-::..                           ",
"   ..:-:::-----------:::--::....:::=***+##++=-:--::..                           ",
"   .:..:=--=--::::....  ..   .:.:====+=+#+-=*+-.:.                             .",
"        .       .             . ..::=++*#+-:::. .                              .",
"                                   ..:.=#:...                                   ",
"                                     ..-#:.                                     ",
"                                       .=. .                                    ",
"                                       .-                                       ",
"                                        ::.                                     ",
"                                        .                                       ",
};

static const int LOGO_ROWS = (int)(sizeof(logo_arte) / sizeof(logo_arte[0]));
static const int LOGO_COLS = 80;

int logo_cols(void) { return LOGO_COLS; }
int logo_rows(void) { return LOGO_ROWS; }

int logo_width_px(int scale_pct)  { return LOGO_COLS * font_cell_w(scale_pct); }
int logo_height_px(int scale_pct) { return LOGO_ROWS * font_cell_h(scale_pct); }

void logo_draw(uint32_t x, uint32_t y, uint32_t color, int scale_pct) {
    uint32_t cw = (uint32_t)font_cell_w(scale_pct);
    uint32_t ch = (uint32_t)font_cell_h(scale_pct);
    for (int row = 0; row < LOGO_ROWS; row++) {
        const char *linea = logo_arte[row];
        for (int col = 0; linea[col] != '\0'; col++) {
            char c = linea[col];
            if (c == ' ') continue;
            font_draw_char(x + (uint32_t)col * cw,
                           y + (uint32_t)row * ch,
                           c, color, COLOR_BLACK, scale_pct);
        }
    }
}
