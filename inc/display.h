#ifndef RKOLED_DISPLAY_H
#define RKOLED_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "font.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_BUFFER_SIZE ((OLED_WIDTH * OLED_HEIGHT) / 8) // 1024 bytes

typedef enum {
    PIXEL_BLACK = 0,
    PIXEL_WHITE = 1
} pixel_color_t;

int display_init(app_config_t *cfg);
void display_close(void);
void display_clear(void);
void display_update(void);
void display_set_contrast(uint8_t val);
void display_turn_on(bool on);
void display_dump_terminal(void); // Render 128x64 buffer as Unicode half-blocks to terminal

// Drawing primitives
void display_draw_pixel(int x, int y, pixel_color_t color);
void display_draw_line(int x0, int y0, int x1, int y1, pixel_color_t color);
void display_draw_rect(int x, int y, int w, int h, pixel_color_t color);
void display_fill_rect(int x, int y, int w, int h, pixel_color_t color);
void display_draw_bitmap(int x, int y, const uint8_t *bitmap, int w, int h, pixel_color_t color);

// Text rendering (handles standard ASCII and UTF-8 single-glyph ℃)
int display_put_char(int x, int y, uint32_t codepoint, const matrix_font_t *font, pixel_color_t color);
int display_put_string(int x, int y, const char *str, const matrix_font_t *font, pixel_color_t color, int max_right);

// XOR character inversion for dual-tone Load bar
int display_put_string_invert(int x, int y, const char *str, const matrix_font_t *font, int max_right);

// Progress bar
void display_draw_progress_bar(int x, int y, int w, int h, float pct);

#endif // RKOLED_DISPLAY_H
