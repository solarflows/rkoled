#ifndef RKOLED_CONFIG_H
#define RKOLED_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DISP_SSD1306 = 0,
    DISP_SH1106  = 1
} display_type_t;

typedef enum {
    STYLE_ASTRA   = 0,  // New Astra high-contrast capsule layout (Default)
    STYLE_CLASSIC = 1,  // Original 4-compartment layout
    STYLE_LUMEN   = 2   // Multi-page card carousel layout
} ui_style_t;

typedef enum {
    VLAN_MODE_AUTO  = 0, // Auto-bind to default gateway VLAN, fallback to first
    VLAN_MODE_CYCLE = 1, // Cycle through all detected active VLANs
    VLAN_MODE_FIXED = 2  // Fixed specified VLAN ID
} vlan_mode_t;

typedef enum {
    NET_DISCON = 0,
    NET_WIFI   = 1,
    NET_ETH    = 2,
    NET_VLAN   = 3
} net_type_t;

typedef struct {
    char i2c_device[64];      // e.g. "/dev/i2c-1"
    uint8_t i2c_addr;         // e.g. 0x3C
    display_type_t disp_type; // SSD1306 or SH1106
    ui_style_t ui_style;      // astra, classic, lumen
    char iface[32];           // Network interface override, or empty for auto
    vlan_mode_t vlan_mode;    // auto, cycle, fixed
    int fixed_vlan_id;        // Used when vlan_mode == VLAN_MODE_FIXED
    char mount_point[64];     // Filesystem path for disk usage, default "/opt" (or "/")
    int refresh_interval;     // Refresh interval in seconds, default 1
    uint8_t contrast;         // Contrast value 0..255, default 0x7F
    bool boot_logo;           // Show "Husky" splash screen on start
    bool daemon_mode;         // Run as background daemon
    bool terminal_mirror;     // Live mirror OLED to terminal console (-T)
    bool dump_once;           // Single-shot snapshot to terminal console and exit (--dump)
    bool dry_run;             // Run without hardware I2C device (--dry-run)
} app_config_t;

void config_set_defaults(app_config_t *cfg);
int config_parse_args(app_config_t *cfg, int argc, char *argv[]);
void config_print_help(const char *prog_name);

#endif // RKOLED_CONFIG_H
