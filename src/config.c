#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void config_set_defaults(app_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->i2c_device[0] = '\0';   // Mandatory: must be explicitly specified via -d / --device
    cfg->i2c_addr = 0x3C;
    cfg->disp_type = DISP_SSD1306;
    cfg->ui_style = STYLE_ASTRA; // Default: new Astra layout
    cfg->iface[0] = '\0';        // Auto-detect default route interface
    cfg->vlan_mode = VLAN_MODE_AUTO;
    cfg->fixed_vlan_id = 0;
    strncpy(cfg->mount_point, "/opt", sizeof(cfg->mount_point) - 1);
    cfg->refresh_interval = 1;   // 1 second
    cfg->contrast = 0x7F;        // Default contrast
    cfg->boot_logo = true;       // Show boot splash by default
    cfg->daemon_mode = false;
    cfg->terminal_mirror = false;
    cfg->dump_once = false;
    cfg->dry_run = false;
}

int config_parse_args(app_config_t *cfg, int argc, char *argv[]) {
    static struct option long_options[] = {
        {"device",    required_argument, 0, 'd'},
        {"addr",      required_argument, 0, 'a'},
        {"type",      required_argument, 0, 't'},
        {"style",     required_argument, 0, 's'},
        {"iface",     required_argument, 0, 'i'},
        {"vlan",      required_argument, 0, 'v'},
        {"mount",     required_argument, 0, 'm'},
        {"rate",      required_argument, 0, 'r'},
        {"contrast",  required_argument, 0, 'c'},
        {"no-boot",   no_argument,       0, 'n'},
        {"daemon",    no_argument,       0, 'D'},
        {"terminal",  no_argument,       0, 'T'},
        {"dump",      no_argument,       0, 'P'},
        {"dry-run",   no_argument,       0, 'X'},
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "d:a:t:s:i:v:m:r:c:nDhVTPX", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd':
                strncpy(cfg->i2c_device, optarg, sizeof(cfg->i2c_device) - 1);
                break;
            case 'a':
                cfg->i2c_addr = (uint8_t)strtoul(optarg, NULL, 0);
                break;
            case 't':
                if (strcasecmp(optarg, "sh1106") == 0) {
                    cfg->disp_type = DISP_SH1106;
                } else {
                    cfg->disp_type = DISP_SSD1306;
                }
                break;
            case 's':
                if (strcasecmp(optarg, "classic") == 0) {
                    cfg->ui_style = STYLE_CLASSIC;
                } else if (strcasecmp(optarg, "lumen") == 0) {
                    cfg->ui_style = STYLE_LUMEN;
                } else {
                    cfg->ui_style = STYLE_ASTRA;
                }
                break;
            case 'i':
                strncpy(cfg->iface, optarg, sizeof(cfg->iface) - 1);
                break;
            case 'v':
                if (strcasecmp(optarg, "cycle") == 0) {
                    cfg->vlan_mode = VLAN_MODE_CYCLE;
                } else if (strcasecmp(optarg, "auto") == 0) {
                    cfg->vlan_mode = VLAN_MODE_AUTO;
                } else {
                    cfg->vlan_mode = VLAN_MODE_FIXED;
                    cfg->fixed_vlan_id = atoi(optarg);
                }
                break;
            case 'm':
                strncpy(cfg->mount_point, optarg, sizeof(cfg->mount_point) - 1);
                break;
            case 'r':
                cfg->refresh_interval = atoi(optarg);
                if (cfg->refresh_interval < 1) cfg->refresh_interval = 1;
                break;
            case 'c':
                cfg->contrast = (uint8_t)atoi(optarg);
                break;
            case 'n':
                cfg->boot_logo = false;
                break;
            case 'D':
                cfg->daemon_mode = true;
                break;
            case 'T':
                cfg->terminal_mirror = true;
                break;
            case 'P':
                cfg->dump_once = true;
                break;
            case 'X':
                cfg->dry_run = true;
                break;
            case 'V':
                printf("rkoled version %s (universal low-overhead daemon)\n", RKOLED_VERSION);
                exit(0);
            case 'h':
            default:
                config_print_help(argv[0]);
                exit(0);
        }
    }

    // Mandatory parameter validation: I2C device node must be explicitly specified
    if (!cfg->dry_run && cfg->i2c_device[0] == '\0') {
        fprintf(stderr, "\n============================================================\n");
        fprintf(stderr, "[CRITICAL ERROR] Mandatory parameter missing: I2C device (-d / --device)!\n");
        fprintf(stderr, "------------------------------------------------------------\n");
        fprintf(stderr, "Reason: Direct I2C hardware access requires explicit bus specification\n");
        fprintf(stderr, "        to prevent accidental collisions or damage to sensitive chips\n");
        fprintf(stderr, "        (such as PMIC regulators on RK356x /dev/i2c-0).\n\n");
        fprintf(stderr, "Usage: %s -d <device> [-a <addr>] [-t <type>] [-s <style>]\n", argv[0]);
        fprintf(stderr, "Example: %s -d /dev/i2c-1 -a 0x3c -t ssd1306 -s astra\n", argv[0]);
        fprintf(stderr, "         %s -X (for software simulation without hardware)\n", argv[0]);
        fprintf(stderr, "Run '%s --help' for all available options.\n", argv[0]);
        fprintf(stderr, "============================================================\n\n");
        return -1;
    }

    return 0;
}

void config_print_help(const char *prog_name) {
    printf("rkoled %s - Universal High-Density Low-Overhead OLED System Monitor\n\n", RKOLED_VERSION);
    printf("Usage: %s -d <device> [OPTIONS]\n\n", prog_name);
    printf("Hardware Options:\n");
    printf("  -d, --device <path>     I2C device path (REQUIRED, e.g. /dev/i2c-1)\n");
    printf("  -a, --addr <hex>        I2C slave address (default: 0x3c, valid: 0x3c, 0x3d)\n");
    printf("  -t, --type <type>       Display controller (ssd1306, sh1106; default: ssd1306)\n");
    printf("  -c, --contrast <0-255>  OLED display contrast (default: 127)\n\n");
    printf("Display & UI Options:\n");
    printf("  -s, --style <name>      UI style: 'astra' (default), 'classic' (original 4-box), 'lumen'\n");
    printf("  -n, --no-boot           Skip the 'Husky' splash screen on startup\n");
    printf("  -r, --rate <sec>        Telemetry refresh interval in seconds (default: 1)\n\n");
    printf("Network & Storage Options:\n");
    printf("  -i, --iface <name>      Network interface (default: auto-detect default route)\n");
    printf("  -v, --vlan <mode|id>    VLAN mode: 'auto' (default route), 'cycle' (rotate), or fixed ID (e.g. 10)\n");
    printf("  -m, --mount <path>      Filesystem mount point for disk usage (default: /opt)\n\n");
    printf("Daemon & Headless Mirror Options:\n");
    printf("  -D, --daemon            Run in background as a system daemon\n");
    printf("  -T, --terminal          Live-mirror OLED screen to terminal console (ideal for remote headless check)\n");
    printf("  -P, --dump              Capture single frame snapshot to terminal console and exit\n");
    printf("  -X, --dry-run           Run without hardware I2C device (for remote software simulation)\n");
    printf("  -V, --version           Print version information and exit\n");
    printf("  -h, --help              Print this help message and exit\n\n");
}
