#include "ui.h"
#include "display.h"
#include "icons.h"
#include "font.h"
#include <stdio.h>
#include <string.h>

void ui_render_astra(const system_telemetry_t *t, const app_config_t *cfg) {
    (void)cfg;
    display_clear();

    // 1. Vertical separator line X=63 (Y: 0..49)
    display_draw_line(63, 0, 63, 49, PIXEL_WHITE);

    // ----------------------------------------------------
    // 2. Left Column (X: 0..62) - Four Badges (All strictly 21px width!)
    // ----------------------------------------------------
    char str_buf[64];

    // [CPU]
    display_fill_rect(1, 1, 21, 9, PIXEL_WHITE);
    display_put_string(2, 2, "CPU", &FONT_6x8, PIXEL_BLACK, 22);
    snprintf(str_buf, sizeof(str_buf), "%.1f%%", t->cpu_usage);
    display_put_string(24, 2, str_buf, &FONT_6x8, PIXEL_WHITE, 62);

    // [MEM]
    display_fill_rect(1, 13, 21, 9, PIXEL_WHITE);
    display_put_string(2, 14, "MEM", &FONT_6x8, PIXEL_BLACK, 22);
    snprintf(str_buf, sizeof(str_buf), "%d%%", t->mem_usage_pct);
    display_put_string(24, 14, str_buf, &FONT_6x8, PIXEL_WHITE, 62);

    // [TMP]
    display_fill_rect(1, 25, 21, 9, PIXEL_WHITE);
    display_put_string(2, 26, "TMP", &FONT_6x8, PIXEL_BLACK, 22);
    // Value with native single-cell ℃ character!
    snprintf(str_buf, sizeof(str_buf), "%.1f℃", t->cpu_temp);
    display_put_string(24, 26, str_buf, &FONT_6x8, PIXEL_WHITE, 62);

    // [DSK] - Strictly 21px width (X: 1..21), zero text crushing!
    display_fill_rect(1, 37, 21, 9, PIXEL_WHITE);
    display_put_string(2, 38, "DSK", &FONT_6x8, PIXEL_BLACK, 22);
    snprintf(str_buf, sizeof(str_buf), "%.1f%%", t->disk_usage_pct);
    display_put_string(24, 38, str_buf, &FONT_6x8, PIXEL_WHITE, 62);

    // ----------------------------------------------------
    // 3. Right Column (X: 64..127) - Strict Anti-Overflow Topology
    // ----------------------------------------------------
    // Select 16x16 icon
    const uint8_t *icon_bmp = ICON_NET_ERROR;
    if (t->net_type == NET_VLAN)      icon_bmp = ICON_VLAN;
    else if (t->net_type == NET_ETH)  icon_bmp = ICON_ETH;
    else if (t->net_type == NET_WIFI) icon_bmp = ICON_WIFI;

    // Draw 16x16 icon at (64, 1)
    display_draw_bitmap(64, 1, icon_bmp, 16, 16, PIXEL_WHITE);

    // Row 1 (Y=2): Label beside icon (X: 82..124)
    if (t->net_type == NET_VLAN) {
        int vid = 0;
        if (t->vlan_count > 0) {
            vid = t->vlans[t->current_vlan_idx].id;
        }
        snprintf(str_buf, sizeof(str_buf), "VLAN %d", vid);
        display_put_string(82, 2, str_buf, &FONT_6x8, PIXEL_WHITE, 126);
    } else if (t->net_type == NET_ETH) {
        display_put_string(82, 2, "ETH 1G", &FONT_6x8, PIXEL_WHITE, 126);
    } else if (t->net_type == NET_WIFI) {
        display_put_string(82, 2, "WI-FI", &FONT_6x8, PIXEL_WHITE, 126);
    } else {
        display_put_string(82, 2, "DISCON", &FONT_6x8, PIXEL_WHITE, 126);
    }

    // Row 2 (Y=10): Mode / Carousel index beside icon (X: 82..112, 15px safety gap!)
    if (t->net_type == NET_VLAN) {
        if (cfg->vlan_mode == VLAN_MODE_CYCLE && t->vlan_count > 1) {
            snprintf(str_buf, sizeof(str_buf), "[%d/%d]", t->current_vlan_idx + 1, t->vlan_count);
            display_put_string(82, 10, str_buf, &FONT_6x8, PIXEL_WHITE, 126);
        } else {
            display_put_string(82, 10, "STATIC", &FONT_6x8, PIXEL_WHITE, 126);
        }
    } else if (t->net_type == NET_ETH) {
        display_put_string(82, 10, "[UP]", &FONT_6x8, PIXEL_WHITE, 126);
    } else if (t->net_type == NET_WIFI) {
        display_put_string(82, 10, "LINK", &FONT_6x8, PIXEL_WHITE, 126);
    } else {
        display_put_string(82, 10, "DOWN", &FONT_6x8, PIXEL_WHITE, 126);
    }

    // Row 3 (Y=19): Full-width interface name (X: 65..110)
    display_put_string(65, 19, t->active_ifname[0] ? t->active_ifname : "none", &FONT_6x8, PIXEL_WHITE, 126);

    // Row 4 (Y=29): Up rate (X: 65..119)
    snprintf(str_buf, sizeof(str_buf), "^%.1f%s", t->tx_rate, t->tx_unit);
    display_put_string(65, 29, str_buf, &FONT_6x8, PIXEL_WHITE, 126);

    // Row 5 (Y=39): Down rate (X: 65..119)
    snprintf(str_buf, sizeof(str_buf), "v%.1f%s", t->rx_rate, t->rx_unit);
    display_put_string(65, 39, str_buf, &FONT_6x8, PIXEL_WHITE, 126);

    // ----------------------------------------------------
    // 4. Bottom Area (Y: 49..63) - System Load Bar + Dual-Tone Inverted IP
    // ----------------------------------------------------
    display_draw_line(0, 49, 127, 49, PIXEL_WHITE);
    display_draw_line(0, 63, 127, 63, PIXEL_WHITE);

    // Background solid Load bar fill
    int bar_w = (int)((t->load_pct / 100.0f) * 126.0f + 0.5f);
    if (bar_w > 126) bar_w = 126;
    if (bar_w > 0) {
        display_fill_rect(1, 50, bar_w, 13, PIXEL_WHITE);
    }

    // Text on top: Invert pixels directly over the progress bar via XOR!
    snprintf(str_buf, sizeof(str_buf), "%s L:%.2f", t->ip_str, t->load_1min);
    display_put_string_invert(4, 53, str_buf, &FONT_6x8, 126);
}
