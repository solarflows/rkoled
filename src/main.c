#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "types.h"
#include "display.h"
#include "collector.h"
#include "ui.h"

static volatile bool s_running = true;

static void sig_handler(int sig) {
    (void)sig;
    s_running = false;
}

static void fatal_crash_handler(int sig) {
    const char *desc = "Unknown Signal";
    if (sig == SIGSEGV) desc = "SIGSEGV (Memory Segmentation Fault)";
#ifdef SIGBUS
    else if (sig == SIGBUS) desc = "SIGBUS (Hardware Bus Alignment Fault)";
#endif
    else if (sig == SIGFPE) desc = "SIGFPE (Arithmetic Exception)";
    else if (sig == SIGILL) desc = "SIGILL (Illegal Instruction)";
    else if (sig == SIGABRT) desc = "SIGABRT (Aborted)";

    fprintf(stderr, "\n============================================================\n");
    fprintf(stderr, "[CRITICAL FAULT] rkoled caught %s (signal %d)!\n", desc, sig);
    fprintf(stderr, "[CRITICAL FAULT] Performing emergency display shutdown to protect hardware...\n");
    fprintf(stderr, "============================================================\n\n");
    fflush(stderr);

    display_close();
    _exit(128 + sig);
}

int main(int argc, char *argv[]) {
    app_config_t cfg;
    config_set_defaults(&cfg);
    config_parse_args(&cfg, argc, argv);

    // Daemonize if requested
#ifdef __linux__
    if (cfg.daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid > 0) exit(EXIT_SUCCESS); // Parent exits
        setsid();
        // Redirect standard file descriptors to /dev/null
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }
#endif

    // Ignore SIGPIPE to avoid dying on broken pipes / sockets
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    // Set up graceful termination signal handlers
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
#ifdef SIGHUP
    signal(SIGHUP, sig_handler);
#endif

    // Catch fatal signals defensively: report cleanly and turn off OLED
    signal(SIGSEGV, fatal_crash_handler);
    signal(SIGFPE,  fatal_crash_handler);
    signal(SIGILL,  fatal_crash_handler);
    signal(SIGABRT, fatal_crash_handler);
#ifdef SIGBUS
    signal(SIGBUS,  fatal_crash_handler);
#endif

    // Initialize display hardware
    if (display_init(&cfg) != 0) {
        fprintf(stderr, "Error: Could not initialize display on %s (0x%02X)\n",
                cfg.i2c_device, cfg.i2c_addr);
        return 1;
    }

    // Initialize telemetry collector
    if (collector_init(&cfg) != 0) {
        fprintf(stderr, "Warning: Collector init incomplete, continuing with defaults\n");
    }

    system_telemetry_t telemetry;
    memset(&telemetry, 0, sizeof(telemetry));

    // Initial sampling delay (100ms) so first CPU delta is calculated
    struct timespec init_sleep = {0, 100000000L}; // 100ms
    nanosleep(&init_sleep, NULL);

    // Single-shot dump mode (--dump / -P) for remote instant visual check
    if (cfg.dump_once) {
        collector_update(&telemetry, &cfg);
        if (cfg.ui_style == STYLE_CLASSIC) {
            ui_render_classic(&telemetry, &cfg);
        } else {
            ui_render_astra(&telemetry, &cfg);
        }
        display_dump_terminal();
        collector_cleanup();
        return 0;
    }

    // Show boot splash screen if enabled
    if (cfg.boot_logo) {
        ui_render_boot(&cfg);
    }

    while (s_running) {
        // 1. Gather all system telemetry in microsecond-scale non-blocking operations
        collector_update(&telemetry, &cfg);

        // 2. Render UI according to selected style
        if (cfg.ui_style == STYLE_CLASSIC) {
            ui_render_classic(&telemetry, &cfg);
        } else {
            // Default: Astra high-contrast capsule layout
            ui_render_astra(&telemetry, &cfg);
        }

        // 3. Push framebuffer to OLED GDDRAM
        display_update();

        // 4. Live mirror to terminal if requested (-T / --terminal)
        if (cfg.terminal_mirror) {
            display_dump_terminal();
        }

        // 5. Sleep for the configured refresh interval
        for (int i = 0; i < cfg.refresh_interval && s_running; i++) {
            sleep(1);
        }
    }

    // Graceful cleanup: clear screen and turn off display panel
    display_close();
    collector_cleanup();

    return 0;
}
