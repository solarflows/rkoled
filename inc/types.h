#ifndef RKOLED_TYPES_H
#define RKOLED_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define MAX_VLANS 8

typedef struct {
    int id;
    char name[32];
    char ip[32];
    bool has_default_gw;
} vlan_info_t;

typedef struct {
    // Compute & Memory
    float cpu_usage;          // 0.0 ~ 100.0 %
    int mem_usage_pct;        // 0 ~ 100 %
    long mem_total_kb;
    long mem_used_kb;
    float cpu_temp;           // in Celsius (e.g. 47.4)
    float disk_usage_pct;     // 0.0 ~ 100.0 %

    // System Load
    double load_1min;
    double load_5min;
    double load_15min;
    float load_pct;           // Normalized load based on CPU core count (0 ~ 100 %)

    // Network Telemetry
    net_type_t net_type;      // WIFI, ETH, VLAN, DISCON
    char active_ifname[32];   // e.g. "end0.10" or "eth0" or "wlan0"
    char ip_str[32];          // e.g. "10.0.10.188"
    float tx_rate;            // In scaled unit (e.g. 192.6)
    float rx_rate;            // In scaled unit (e.g. 610.1)
    const char *tx_unit;      // "B/s", "K/s", "M/s", "G/s"
    const char *rx_unit;

    // VLAN state
    int vlan_count;
    int current_vlan_idx;     // For cycle mode (0..vlan_count-1)
    vlan_info_t vlans[MAX_VLANS];

    // Status
    uint64_t uptime_sec;
    uint32_t tick_count;
} system_telemetry_t;

#endif // RKOLED_TYPES_H
