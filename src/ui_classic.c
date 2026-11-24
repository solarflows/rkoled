#include "ui.h"
#include "display.h"
#include "icons.h"
#include "font.h"
#include <stdio.h>

void ui_render_classic(const system_telemetry_t *t, const app_config_t *cfg) {
    (void)cfg;
    display_clear();

    // 1. Original 6 compartment framing lines
    display_draw_line(0, 0, 127, 0, PIXEL_WHITE);       // Top border Y=0
    display_draw_line(0, 15, 127, 15, PIXEL_WHITE);     // Header separator Y=15
    display_draw_line(82, 15, 82, 38, PIXEL_WHITE);     // Mid vertical separator X=82
    display_draw_line(0, 38, 127, 38, PIXEL_WHITE);     // Mid-bottom separator Y=38
    display_draw_line(0, 63, 127, 63, PIXEL_WHITE);     // Bottom border Y=63
    display_draw_line(37, 39, 37, 62, PIXEL_WHITE);     // Bottom vertical separator X=37

    // 2. Top Header (Y: 0..15)
    const uint8_t *icon_bmp = ICON_NET_ERROR;
    if (t->net_type == NET_VLAN)      icon_bmp = ICON_VLAN;
    else if (t->net_type == NET_ETH)  icon_bmp = ICON_ETH;
    else if (t->net_type == NET_WIFI) icon_bmp = ICON_WIFI;

    display_draw_bitmap(0, 0, icon_bmp, 16, 16, PIXEL_WHITE);
    display_put_string(17, 4, t->ip_str, &FONT_7x10, PIXEL_WHITE, 128);

    char buf[32];

    // 3. Middle Left (X: 0..81, Y: 15..38)
    snprintf(buf, sizeof(buf), "CPU: %.1f%%", t->cpu_usage);
    display_put_string(0, 17, buf, &FONT_7x10, PIXEL_WHITE, 82);

    snprintf(buf, sizeof(buf), "Mem: %d%%", t->mem_usage_pct);
    display_put_string(0, 28, buf, &FONT_7x10, PIXEL_WHITE, 82);

    // 4. Middle Right (X: 83..127, Y: 15..38)
    display_put_string(91, 17, "Temp", &FONT_7x10, PIXEL_WHITE, 128);
    snprintf(buf, sizeof(buf), "%.1fC", t->cpu_temp);
    display_put_string(88, 28, buf, &FONT_7x10, PIXEL_WHITE, 128);

    // 5. Bottom Left (X: 0..36, Y: 39..62)
    display_put_string(7, 42, "Disk", &FONT_6x8, PIXEL_WHITE, 37);
    snprintf(buf, sizeof(buf), "%.1f%%", t->disk_usage_pct);
    display_put_string(4, 52, buf, &FONT_6x8, PIXEL_WHITE, 37);

    // 6. Bottom Right (X: 38..127, Y: 39..62)
    snprintf(buf, sizeof(buf), " Up :%.1f %s", t->tx_rate, t->tx_unit);
    display_put_string(39, 42, buf, &FONT_6x8, PIXEL_WHITE, 128);

    snprintf(buf, sizeof(buf), "Down:%.1f %s", t->rx_rate, t->rx_unit);
    display_put_string(39, 52, buf, &FONT_6x8, PIXEL_WHITE, 128);
}
