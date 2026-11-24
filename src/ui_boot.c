#include "ui.h"
#include "display.h"
#include "font.h"
#include <unistd.h>

void ui_render_boot(const app_config_t *cfg) {
    (void)cfg;
    display_clear();

    // Top solid decorative bar
    display_fill_rect(0, 0, 128, 5, PIXEL_WHITE);

    // Board identifier
    display_put_string(21, 2, "Panther X2", &FONT_6x8, PIXEL_BLACK, 128);

    // Custom "Husky" emblem in large 16x26 font!
    display_put_string(15, 16, "Husky", &FONT_16x26, PIXEL_WHITE, 128);

    // Bottom system tag
    display_put_string(15, 52, "HomeAssistant", &FONT_6x8, PIXEL_WHITE, 128);

    display_update();
    sleep(2);
}
