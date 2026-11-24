#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#else
// Non-Linux mock fallback for compilation testing
#define I2C_SLAVE 0x0703
static int ioctl(int fd, unsigned long req, ...) { (void)fd; (void)req; return 0; }
#endif

static int s_i2c_fd = -1;
static uint8_t s_buffer[OLED_BUFFER_SIZE]; // 1024 bytes GDDRAM
static display_type_t s_disp_type = DISP_SSD1306;
static int s_consecutive_i2c_errors = 0;

static inline int write_cmd(uint8_t cmd) {
    if (s_i2c_fd < 0) return 0;
    uint8_t buf[2] = {0x00, cmd};
    return (write(s_i2c_fd, buf, 2) == 2) ? 0 : -1;
}

static void handle_i2c_error(const char *op) {
    s_consecutive_i2c_errors++;
    if (s_consecutive_i2c_errors == 1) {
        fprintf(stderr, "[ERROR] I2C bus transmission failed (%s): %s (errno: %d). Suppressing repeated logs.\n",
                op, strerror(errno), errno);
    }
}

static void handle_i2c_success(void) {
    if (s_consecutive_i2c_errors > 0) {
        fprintf(stderr, "[NOTICE] I2C communication successfully recovered after %d failed attempts!\n",
                s_consecutive_i2c_errors);
        s_consecutive_i2c_errors = 0;
    }
}

// Probes the specified I2C device and slave address for hardware ACK
static int probe_i2c_slave(const char *dev_path, uint8_t addr) {
    int fd = open(dev_path, O_RDWR);
    if (fd < 0) return -1;

#ifdef __linux__
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        close(fd);
        return -1;
    }
    // Probe with a display-off command (0xAE)
    uint8_t probe_buf[2] = {0x00, 0xAE};
    int ret = write(fd, probe_buf, 2);
    close(fd);
    return (ret == 2) ? 0 : -1;
#else
    (void)addr;
    close(fd);
    return 0;
#endif
}

int display_init(app_config_t *cfg) {
    s_disp_type = cfg->disp_type;
    memset(s_buffer, 0, sizeof(s_buffer));

    // ========================================================
    // HARDWARE SAFETY INVARIANT 0: Mandatory Device Specification
    // Refuse access immediately if I2C bus device path is empty.
    // ========================================================
    if (cfg->i2c_device[0] == '\0' && !cfg->dry_run) {
        fprintf(stderr, "\n============================================================\n");
        fprintf(stderr, "[HARDWARE SAFETY REFUSAL] No I2C device specified!\n");
        fprintf(stderr, "Reason: Explicit device path (-d /dev/i2c-X) is required.\n");
        fprintf(stderr, "============================================================\n\n");
        return -1;
    }

    // ========================================================
    // HARDWARE SAFETY INVARIANT 1: PMIC Bus Blacklist
    // On Rockchip RK356x / Panther-X2, i2c-0 hosts the RK809 PMIC
    // and TCS4525 CPU voltage regulators. Refuse access immediately.
    // ========================================================
    if (strstr(cfg->i2c_device, "i2c-0") || strstr(cfg->i2c_device, "i2c0")) {
        fprintf(stderr, "\n============================================================\n");
        fprintf(stderr, "[HARDWARE SAFETY REFUSAL] Access to i2c-0 is strictly prohibited!\n");
        fprintf(stderr, "Reason: i2c-0 is the dedicated internal bus for RK809 PMIC & CPU regulators.\n");
        fprintf(stderr, "Touching i2c-0 could trigger power rail resets or hardware damage.\n");
        fprintf(stderr, "Please connect OLED to i2c-1 or i2c-3, and run with: -d /dev/i2c-1\n");
        fprintf(stderr, "============================================================\n\n");
        return -1;
    }

    // ========================================================
    // HARDWARE SAFETY INVARIANT 2: Strict OLED Slave Whitelist
    // SSD1306/SSD1315/SH1106 ONLY operate on 0x3C or 0x3D.
    // Refuse touching any other bus address (PMIC is at 0x20).
    // ========================================================
    if (cfg->i2c_addr != 0x3C && cfg->i2c_addr != 0x3D) {
        fprintf(stderr, "\n============================================================\n");
        fprintf(stderr, "[HARDWARE SAFETY REFUSAL] Invalid slave address 0x%02X!\n", cfg->i2c_addr);
        fprintf(stderr, "Reason: OLED controllers only use 0x3C or 0x3D.\n");
        fprintf(stderr, "Access to other addresses is blocked to protect on-board peripherals.\n");
        fprintf(stderr, "============================================================\n\n");
        return -1;
    }

    if (cfg->dry_run || cfg->dump_once) {
        // In dry-run mode, allow proceeding even without real I2C hardware
        s_i2c_fd = open(cfg->i2c_device, O_RDWR);
        if (s_i2c_fd >= 0) {
            ioctl(s_i2c_fd, I2C_SLAVE, cfg->i2c_addr);
        }
        return 0;
    }

    // ========================================================
    // HARDWARE SAFETY INVARIANT 3: Strict Scope (No Blind Hopping)
    // Only open the explicitly targeted device. If it fails, report
    // in pure English ASCII and exit cleanly without touching other buses.
    // ========================================================
    s_i2c_fd = open(cfg->i2c_device, O_RDWR);
    if (s_i2c_fd < 0) {
        fprintf(stderr, "\n[I2C ERROR] Cannot open device '%s': %s (errno: %d)\n",
                cfg->i2c_device, strerror(errno), errno);
        fprintf(stderr, "[HINT] Please verify that '%s' exists and user has rw permission.\n", cfg->i2c_device);
        return -1;
    }

    if (ioctl(s_i2c_fd, I2C_SLAVE, cfg->i2c_addr) < 0) {
        fprintf(stderr, "\n[I2C ERROR] Failed to set slave address 0x%02X on '%s': %s\n",
                cfg->i2c_addr, cfg->i2c_device, strerror(errno));
        close(s_i2c_fd);
        s_i2c_fd = -1;
        return -1;
    }

    // Active ACK Probe on the targeted device only
    if (probe_i2c_slave(cfg->i2c_device, cfg->i2c_addr) != 0) {
        fprintf(stderr, "\n============================================================\n");
        fprintf(stderr, "[I2C HARDWARE PROBE FAILED]\n");
        fprintf(stderr, "------------------------------------------------------------\n");
        fprintf(stderr, "Target: %s (slave address: 0x%02X)\n", cfg->i2c_device, cfg->i2c_addr);
        fprintf(stderr, "Status: No ACK response received from OLED chip.\n");
        if (errno != 0) {
            fprintf(stderr, "Kernel Error: %s (errno: %d)\n", strerror(errno), errno);
        }
        fprintf(stderr, "\nTroubleshooting Checklist:\n");
        fprintf(stderr, "  1. Wiring: Ensure VCC, GND, SDA, SCL are securely connected.\n");
        fprintf(stderr, "  2. I2C Bus: If connected to i2c-3 instead of i2c-1, run:\n");
        fprintf(stderr, "       rkoled -d /dev/i2c-3\n");
        fprintf(stderr, "  3. Scan: Run 'sudo i2cdetect -y <bus_num>' to verify chip ACK.\n");
        fprintf(stderr, "  4. Headless simulation: Run with '--dump' or '-X (--dry-run)'.\n");
        fprintf(stderr, "============================================================\n\n");
        close(s_i2c_fd);
        s_i2c_fd = -1;
        return -1;
    }

    // Turn off display during configuration
    write_cmd(0xAE);

    if (s_disp_type == DISP_SSD1306) {
        write_cmd(0xD5); // Set Display Clock Divide Ratio
        write_cmd(0x80);
        write_cmd(0xA8); // Set Multiplex Ratio
        write_cmd(0x3F); // 64MUX
        write_cmd(0xD3); // Set Display Offset
        write_cmd(0x00);
        write_cmd(0x40); // Set Display Start Line (0)
        write_cmd(0x8D); // Charge Pump Setting
        write_cmd(0x14); // Enable Charge Pump
        write_cmd(0x20); // Set Memory Addressing Mode
        write_cmd(0x00); // 0x00 = Horizontal Addressing Mode! (High throughput)
        write_cmd(0xA1); // Column Address 127 is mapped to SEG0 (H-flip)
        write_cmd(0xC8); // COM Output Scan Direction (V-flip)
        write_cmd(0xDA); // Set COM Pins Hardware Config
        write_cmd(0x12);
        write_cmd(0x81); // Contrast Control
        write_cmd(cfg->contrast);
        write_cmd(0xD9); // Set Pre-charge Period
        write_cmd(0xF1);
        write_cmd(0xDB); // Set VCOMH Deselect Level
        write_cmd(0x30);
        write_cmd(0xA4); // Output Follows RAM Content
        write_cmd(0xA6); // Normal Display (non-inverted)
        write_cmd(0xAF); // Turn Display ON
    } else {
        // SH1106 Initialization
        write_cmd(0x02); // Set Lower Column Address (Offset 2 for SH1106 132x64 panel)
        write_cmd(0x10); // Set Higher Column Address
        write_cmd(0x40); // Set Display Start Line (0)
        write_cmd(0xB0); // Set Page Address
        write_cmd(0x81); // Contrast Control
        write_cmd(cfg->contrast);
        write_cmd(0xA1); // Segment Remap
        write_cmd(0xC8); // Common Scan Direction
        write_cmd(0xA6); // Normal Display
        write_cmd(0xA8); // Multiplex Ratio
        write_cmd(0x3F);
        write_cmd(0xD3); // Display Offset
        write_cmd(0x00);
        write_cmd(0xD5); // Clock Divide Ratio
        write_cmd(0x80);
        write_cmd(0xD9); // Dis-charge / Pre-charge
        write_cmd(0xF1);
        write_cmd(0xDA); // COM Pins Configuration
        write_cmd(0x12);
        write_cmd(0xDB); // VCOM Deselect Level
        write_cmd(0x40);
        write_cmd(0x8D); // Charge Pump
        write_cmd(0x14);
        write_cmd(0xAF); // Turn Display ON
    }

    display_clear();
    display_update();
    return 0;
}

void display_close(void) {
    if (s_i2c_fd >= 0) {
        display_clear();
        display_update();
        write_cmd(0xAE); // Turn off OLED display to prevent burn-in
        close(s_i2c_fd);
        s_i2c_fd = -1;
    }
}

void display_clear(void) {
    memset(s_buffer, 0x00, sizeof(s_buffer));
}

void display_set_contrast(uint8_t val) {
    write_cmd(0x81);
    write_cmd(val);
}

void display_turn_on(bool on) {
    write_cmd(on ? 0xAF : 0xAE);
}

// Renders the 128x64 in-memory GDDRAM buffer to the terminal console using Unicode half-blocks
void display_dump_terminal(void) {
    printf("+--------------------------------------------------------------------------------------------------------------------------------+\n");
    for (int y = 0; y < 64; y += 2) {
        putchar('|');
        for (int x = 0; x < 128; x++) {
            int p0 = (s_buffer[(y / 8) * 128 + x] >> (y % 8)) & 1;
            int p1 = (s_buffer[((y + 1) / 8) * 128 + x] >> ((y + 1) % 8)) & 1;
            if (p0 && p1) printf("\u2588");
            else if (p0 && !p1) printf("\u2580");
            else if (!p0 && p1) printf("\u2584");
            else putchar(' ');
        }
        printf("|\n");
    }
    printf("+--------------------------------------------------------------------------------------------------------------------------------+\n");
    fflush(stdout);
}

// Ultra-efficient chunked GDDRAM buffer write with error trapping
void display_update(void) {
    if (s_i2c_fd < 0) return;

    if (s_disp_type == DISP_SSD1306) {
        // Horizontal addressing: reset column and page range once
        if (write_cmd(0x21) < 0 || write_cmd(0x00) < 0 || write_cmd(0x7F) < 0 ||
            write_cmd(0x22) < 0 || write_cmd(0x00) < 0 || write_cmd(0x07) < 0) {
            handle_i2c_error("SSD1306 addressing commands");
            return;
        }

        // Stream 1024 bytes in 64-byte chunks with 0x40 (data mode)
        uint8_t tx_buf[65];
        tx_buf[0] = 0x40; // Co=0, D/C#=1 (Data Mode)

        for (int i = 0; i < OLED_BUFFER_SIZE; i += 64) {
            memcpy(&tx_buf[1], &s_buffer[i], 64);
            if (write(s_i2c_fd, tx_buf, 65) != 65) {
                handle_i2c_error("GDDRAM streaming");
                return; // Abort this frame immediately, don't spam bus!
            }
        }
        handle_i2c_success();
    } else {
        // SH1106: Page-by-page streaming with offset 2
        uint8_t tx_buf[133];
        tx_buf[0] = 0x40;

        for (uint8_t p = 0; p < 8; p++) {
            if (write_cmd(0xB0 + p) < 0 || write_cmd(0x02) < 0 || write_cmd(0x10) < 0) {
                handle_i2c_error("SH1106 page setup");
                return;
            }
            memcpy(&tx_buf[1], &s_buffer[p * 128], 128);
            if (write(s_i2c_fd, tx_buf, 129) != 129) {
                handle_i2c_error("SH1106 page streaming");
                return;
            }
        }
        handle_i2c_success();
    }
}

void display_draw_pixel(int x, int y, pixel_color_t color) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * OLED_WIDTH + x;

    if (color == PIXEL_WHITE) {
        s_buffer[idx] |= (1 << bit);
    } else {
        s_buffer[idx] &= ~(1 << bit);
    }
}

void display_draw_line(int x0, int y0, int x1, int y1, pixel_color_t color) {
    if (x0 == x1) {
        int sy = (y0 < y1) ? y0 : y1;
        int ey = (y0 > y1) ? y0 : y1;
        for (int y = sy; y <= ey; y++) display_draw_pixel(x0, y, color);
    } else if (y0 == y1) {
        int sx = (x0 < x1) ? x0 : x1;
        int ex = (x0 > x1) ? x0 : x1;
        for (int x = sx; x <= ex; x++) display_draw_pixel(x, y0, color);
    } else {
        // Bresenham's line algorithm
        int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
        int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy, e2;

        while (1) {
            display_draw_pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

void display_draw_rect(int x, int y, int w, int h, pixel_color_t color) {
    display_draw_line(x, y, x + w - 1, y, color);
    display_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    display_draw_line(x, y, x, y + h - 1, color);
    display_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void display_fill_rect(int x, int y, int w, int h, pixel_color_t color) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            display_draw_pixel(x + i, y + j, color);
        }
    }
}

void display_draw_bitmap(int x, int y, const uint8_t *bitmap, int w, int h, pixel_color_t color) {
    int byte_width = (w + 7) / 8;
    uint8_t byte_val = 0;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (i & 7) {
                byte_val <<= 1;
            } else {
                byte_val = bitmap[j * byte_width + (i / 8)];
            }
            if (byte_val & 0x80) {
                display_draw_pixel(x + i, y + j, color);
            }
        }
    }
}

int display_put_char(int x, int y, uint32_t codepoint, const matrix_font_t *font, pixel_color_t color) {
    const uint16_t *glyph = NULL;

    // Intercept single-glyph Celsius (℃) and degree ring (°)
    if (codepoint == 0x2103 && font->celsius_glyph) {
        glyph = font->celsius_glyph;
    } else if (codepoint == 0x00B0 && font->degree_glyph) {
        glyph = font->degree_glyph;
    } else {
        if (codepoint < 32 || codepoint > 126) return 0;
        glyph = &font->bitmap[(codepoint - 32) * font->height];
    }

    if (x + font->width > OLED_WIDTH || y + font->height > OLED_HEIGHT) return 0;

    for (int i = 0; i < font->height; i++) {
        uint16_t row_val = glyph[i];
        for (int j = 0; j < font->width; j++) {
            bool bit = ((row_val << j) & 0x8000) != 0;
            display_draw_pixel(x + j, y + i, bit ? color : (color ? PIXEL_BLACK : PIXEL_WHITE));
        }
    }

    return font->width;
}

// Parses UTF-8 stream to handle standard ASCII and multibyte ℃ (U+2103) & ° (U+00B0)
int display_put_string(int x, int y, const char *str, const matrix_font_t *font, pixel_color_t color, int max_right) {
    if (!str || !font) return x;
    int cur_x = x;
    const unsigned char *p = (const unsigned char *)str;

    while (*p) {
        uint32_t cp = 0;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && *(p + 1)) {
            cp = ((*p & 0x1F) << 6) | (*(p + 1) & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && *(p + 1) && *(p + 2)) {
            cp = ((*p & 0x0F) << 12) | ((*(p + 1) & 0x3F) << 6) | (*(p + 2) & 0x3F);
            p += 3;
        } else {
            p++;
            continue;
        }

        if (cur_x + font->width > max_right) break;
        cur_x += display_put_char(cur_x, y, cp, font, color);
    }

    return cur_x;
}

// XOR character inversion for dual-tone Load bar: inverts screen pixels directly under character strokes!
int display_put_string_invert(int x, int y, const char *str, const matrix_font_t *font, int max_right) {
    if (!str || !font) return x;
    int cur_x = x;
    const unsigned char *p = (const unsigned char *)str;

    while (*p) {
        uint32_t cp = 0;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && *(p + 1)) {
            cp = ((*p & 0x1F) << 6) | (*(p + 1) & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && *(p + 1) && *(p + 2)) {
            cp = ((*p & 0x0F) << 12) | ((*(p + 1) & 0x3F) << 6) | (*(p + 2) & 0x3F);
            p += 3;
        } else {
            p++;
            continue;
        }

        if (cur_x + font->width > max_right) break;

        const uint16_t *glyph = NULL;
        if (cp == 0x2103 && font->celsius_glyph) {
            glyph = font->celsius_glyph;
        } else if (cp == 0x00B0 && font->degree_glyph) {
            glyph = font->degree_glyph;
        } else if (cp >= 32 && cp <= 126) {
            glyph = &font->bitmap[(cp - 32) * font->height];
        }

        if (glyph) {
            for (int r = 0; r < font->height; r++) {
                uint16_t row_val = glyph[r];
                for (int c = 0; c < font->width; c++) {
                    if ((row_val << c) & 0x8000) {
                        int px = cur_x + c;
                        int py = y + r;
                        if (px >= 0 && px < OLED_WIDTH && py >= 0 && py < OLED_HEIGHT) {
                            int page = py / 8;
                            int bit  = py % 8;
                            s_buffer[page * OLED_WIDTH + px] ^= (1 << bit); // XOR invert pixel!
                        }
                    }
                }
            }
        }

        cur_x += font->width;
    }

    return cur_x;
}

void display_draw_progress_bar(int x, int y, int w, int h, float pct) {
    display_draw_rect(x, y, w, h, PIXEL_WHITE);
    int inner_w = w - 4;
    int fill_w = (int)(((pct < 0 ? 0 : (pct > 100 ? 100 : pct)) / 100.0f) * inner_w + 0.5f);
    if (fill_w > 0 && h > 4) {
        display_fill_rect(x + 2, y + 2, fill_w, h - 4, PIXEL_WHITE);
    }
}
