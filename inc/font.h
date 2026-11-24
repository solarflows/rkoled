#ifndef RKOLED_FONT_H
#define RKOLED_FONT_H

#include <stdint.h>

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint16_t *bitmap;
    const uint16_t *celsius_glyph; // Dedicated single-cell ℃ glyph
    const uint16_t *degree_glyph;  // Dedicated degree ring ° glyph
} matrix_font_t;

extern const matrix_font_t FONT_6x8;
extern const matrix_font_t FONT_7x10;
extern const matrix_font_t FONT_16x26;

#endif // RKOLED_FONT_H
