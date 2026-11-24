#include "collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef __linux__
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#else
// Non-Linux mock fallback
struct statvfs { unsigned long f_blocks; unsigned long f_bavail; unsigned long f_frsize; };
static int statvfs(const char *p, struct statvfs *s) { (void)p; s->f_blocks = 1000; s->f_bavail = 700; s->f_frsize = 4096; return 0; }
static int getloadavg(double loadavg[], int nelem) { for (int i=0; i<nelem; i++) loadavg[i] = 1.42; return nelem; }
#endif

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} raw_cpu_stat_t;

typedef struct {
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
} raw_net_stat_t;

static raw_cpu_stat_t s_prev_cpu;
static raw_net_stat_t s_prev_net;
static struct timespec s_prev_time;
static int s_num_cores = 4;
static char s_primary_iface[32] = "";

static void read_cpu_stat(raw_cpu_stat_t *st) {
    memset(st, 0, sizeof(*st));
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &st->user, &st->nice, &st->system, &st->idle,
               &st->iowait, &st->irq, &st->softirq, &st->steal);
    }
    fclose(fp);
}

static void read_net_bytes(const char *ifname, raw_net_stat_t *st) {
    st->rx_bytes = 0;
    st->tx_bytes = 0;
    if (!ifname || !ifname[0]) return;

    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return;

    char line[256];
    char target[64];
    snprintf(target, sizeof(target), "%s:", ifname);

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, target);
        if (p) {
            p += strlen(target);
            sscanf(p, "%llu %*u %*u %*u %*u %*u %*u %*u %llu",
                   &st->rx_bytes, &st->tx_bytes);
            break;
        }
    }
    fclose(fp);
}

// Detect default gateway interface from /proc/net/route
static void detect_default_gateway_iface(char *out_iface, size_t max_len) {
    out_iface[0] = '\0';
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) return;

    char line[256];
    char iface[32];
    unsigned long dest, gateway;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%31s %lx %lx", iface, &dest, &gateway) == 3) {
            if (dest == 0) { // Default gateway (0.0.0.0)
                strncpy(out_iface, iface, max_len - 1);
                out_iface[max_len - 1] = '\0';
                break;
            }
        }
    }
    fclose(fp);
}

static int get_local_ip(const char *ifname, char *out_ip, size_t max_len) {
    out_ip[0] = '\0';
    if (!ifname || !ifname[0]) return -1;

#ifdef __linux__
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);

    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
        const char *ip = inet_ntoa(sin->sin_addr);
        if (ip) {
            strncpy(out_ip, ip, max_len - 1);
            out_ip[max_len - 1] = '\0';
            close(sock);
            return 0;
        }
    }
    close(sock);
#else
    strncpy(out_ip, "10.0.10.188", max_len - 1);
    return 0;
#endif
    return -1;
}

static bool check_iface_carrier(const char *ifname) {
    if (!ifname || !ifname[0]) return false;
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", ifname);
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    int carrier = 0;
    if (fscanf(fp, "%d", &carrier) == 1 && carrier == 1) {
        fclose(fp);
        return true;
    }
    fclose(fp);
    return false;
}

// Discover all active VLAN interfaces (e.g. end0.10, eth0.100)
static int discover_vlans(vlan_info_t vlans[], int max_vlans, const char *default_iface) {
    int count = 0;
    DIR *dir = opendir("/sys/class/net");
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_vlans) {
        if (entry->d_name[0] == '.') continue;

        // Check if interface contains a dot (e.g. end0.10, eth0.100) or starts with vlan
        char *dot = strchr(entry->d_name, '.');
        if (dot || strncmp(entry->d_name, "vlan", 4) == 0) {
            int vid = 0;
            if (dot) {
                vid = atoi(dot + 1);
            } else {
                vid = atoi(entry->d_name + 4);
            }

            if (vid > 0) {
                vlans[count].id = vid;
                snprintf(vlans[count].name, sizeof(vlans[count].name), "%.31s", entry->d_name);
                get_local_ip(entry->d_name, vlans[count].ip, sizeof(vlans[count].ip));
                vlans[count].has_default_gw = (default_iface && strcmp(entry->d_name, default_iface) == 0);
                count++;
            }
        }
    }
    closedir(dir);

    // If default gateway VLAN exists, bubble it to index 0 so it serves as primary
    for (int i = 0; i < count; i++) {
        if (vlans[i].has_default_gw && i > 0) {
            vlan_info_t tmp = vlans[0];
            vlans[0] = vlans[i];
            vlans[i] = tmp;
            break;
        }
    }

    return count;
}

int collector_init(const app_config_t *cfg) {
    (void)cfg;
    read_cpu_stat(&s_prev_cpu);
    clock_gettime(CLOCK_MONOTONIC, &s_prev_time);

    // Read CPU core count
#ifdef __linux__
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores > 0) s_num_cores = (int)cores;
#else
    s_num_cores = 4;
#endif

    // Initial default interface
    detect_default_gateway_iface(s_primary_iface, sizeof(s_primary_iface));
    if (s_primary_iface[0]) {
        read_net_bytes(s_primary_iface, &s_prev_net);
    }

    return 0;
}

void collector_update(system_telemetry_t *t, const app_config_t *cfg) {
    if (!t) return;
    t->tick_count++;

    // 1. Calculate time delta
    struct timespec curr_time;
    clock_gettime(CLOCK_MONOTONIC, &curr_time);
    double dt = (curr_time.tv_sec - s_prev_time.tv_sec) +
                (curr_time.tv_nsec - s_prev_time.tv_nsec) / 1e9;
    if (dt <= 0.001) dt = 1.0;
    s_prev_time = curr_time;

    // 2. CPU Usage Calculation (Continuous delta, 0 internal sleep!)
    raw_cpu_stat_t curr_cpu;
    read_cpu_stat(&curr_cpu);

    unsigned long long prev_total = s_prev_cpu.user + s_prev_cpu.nice + s_prev_cpu.system +
                                   s_prev_cpu.idle + s_prev_cpu.iowait + s_prev_cpu.irq +
                                   s_prev_cpu.softirq + s_prev_cpu.steal;
    unsigned long long curr_total = curr_cpu.user + curr_cpu.nice + curr_cpu.system +
                                   curr_cpu.idle + curr_cpu.iowait + curr_cpu.irq +
                                   curr_cpu.softirq + curr_cpu.steal;
    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff  = (curr_cpu.idle + curr_cpu.iowait) - (s_prev_cpu.idle + s_prev_cpu.iowait);

    if (total_diff > 0 && total_diff >= idle_diff) {
        t->cpu_usage = 100.0f * (float)(total_diff - idle_diff) / (float)total_diff;
    } else {
        t->cpu_usage = 0.0f;
    }
    s_prev_cpu = curr_cpu;

    // 3. Memory Usage Calculation
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        long mem_total = 0, mem_free = 0, buffers = 0, cached = 0, sreclaim = 0;
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0)       sscanf(line + 9, "%ld", &mem_total);
            else if (strncmp(line, "MemFree:", 8) == 0)   sscanf(line + 8, "%ld", &mem_free);
            else if (strncmp(line, "Buffers:", 8) == 0)   sscanf(line + 8, "%ld", &buffers);
            else if (strncmp(line, "Cached:", 7) == 0)    sscanf(line + 7, "%ld", &cached);
            else if (strncmp(line, "SReclaimable:", 13)==0) sscanf(line + 13, "%ld", &sreclaim);
        }
        fclose(fp);

        long used = mem_total - mem_free - buffers - cached - sreclaim;
        if (used < 0) used = 0;
        t->mem_total_kb = mem_total;
        t->mem_used_kb  = used;
        t->mem_usage_pct = (mem_total > 0) ? (int)((double)used / mem_total * 100.0 + 0.5) : 0;
    }

    // 4. CPU Temperature Reading
    FILE *tfp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!tfp) tfp = fopen("/sys/class/thermal/thermal_zone1/temp", "r");
    if (tfp) {
        long raw_temp = 0;
        if (fscanf(tfp, "%ld", &raw_temp) == 1) {
            t->cpu_temp = (float)raw_temp / 1000.0f;
        }
        fclose(tfp);
    }

    // 5. Disk Usage Reading
    struct statvfs svfs;
    const char *mount_pt = cfg->mount_point[0] ? cfg->mount_point : "/opt";
    if (statvfs(mount_pt, &svfs) != 0 && strcmp(mount_pt, "/") != 0) {
        statvfs("/", &svfs); // fallback to root
    }
    if (svfs.f_blocks > 0) {
        double total_b = (double)svfs.f_blocks * svfs.f_frsize;
        double avail_b = (double)svfs.f_bavail * svfs.f_frsize;
        t->disk_usage_pct = (float)((1.0 - avail_b / total_b) * 100.0);
    }

    // 6. System Load Average Reading
    double loads[3] = {0};
    if (getloadavg(loads, 3) >= 1) {
        t->load_1min  = loads[0];
        t->load_5min  = loads[1];
        t->load_15min = loads[2];
        t->load_pct   = (float)((t->load_1min / (double)s_num_cores) * 100.0);
        if (t->load_pct > 100.0f) t->load_pct = 100.0f;
        if (t->load_pct < 0.0f)   t->load_pct = 0.0f;
    }

    // 7. Network Interface & VLAN Resolution
    char default_gw_if[32] = "";
    detect_default_gateway_iface(default_gw_if, sizeof(default_gw_if));

    // Discover VLANs
    t->vlan_count = discover_vlans(t->vlans, MAX_VLANS, default_gw_if);

    // Resolve target active interface
    if (cfg->iface[0]) {
        // User explicitly specified interface
        snprintf(t->active_ifname, sizeof(t->active_ifname), "%s", cfg->iface);
    } else if (cfg->vlan_mode == VLAN_MODE_FIXED && cfg->fixed_vlan_id > 0) {
        // User specified fixed VLAN ID
        snprintf(t->active_ifname, sizeof(t->active_ifname), "end0.%d", cfg->fixed_vlan_id);
    } else if (t->vlan_count > 0) {
        // VLANs detected!
        if (cfg->vlan_mode == VLAN_MODE_CYCLE && t->vlan_count > 1) {
            // Cycle every 4 ticks
            t->current_vlan_idx = (t->tick_count / 4) % t->vlan_count;
        } else {
            // Default to primary / default gateway VLAN (index 0)
            t->current_vlan_idx = 0;
        }
        snprintf(t->active_ifname, sizeof(t->active_ifname), "%s", t->vlans[t->current_vlan_idx].name);
    } else if (default_gw_if[0]) {
        // Fall back to default gateway interface
        snprintf(t->active_ifname, sizeof(t->active_ifname), "%s", default_gw_if);
    } else {
        // Fall back to eth0 or wlan0
        if (check_iface_carrier("eth0")) snprintf(t->active_ifname, sizeof(t->active_ifname), "eth0");
        else if (check_iface_carrier("end0")) snprintf(t->active_ifname, sizeof(t->active_ifname), "end0");
        else snprintf(t->active_ifname, sizeof(t->active_ifname), "wlan0");
    }

    // Classify network type
    if (strchr(t->active_ifname, '.') || strncmp(t->active_ifname, "vlan", 4) == 0) {
        t->net_type = NET_VLAN;
    } else if (strncmp(t->active_ifname, "wlan", 4) == 0 || strncmp(t->active_ifname, "ra", 2) == 0) {
        t->net_type = NET_WIFI;
    } else if (strncmp(t->active_ifname, "eth", 3) == 0 || strncmp(t->active_ifname, "end", 3) == 0) {
        t->net_type = NET_ETH;
    } else {
        t->net_type = NET_DISCON;
    }

    // Fetch IP
    if (get_local_ip(t->active_ifname, t->ip_str, sizeof(t->ip_str)) != 0) {
        strncpy(t->ip_str, "0.0.0.0", sizeof(t->ip_str) - 1);
        if (t->net_type != NET_WIFI && !check_iface_carrier(t->active_ifname)) {
            t->net_type = NET_DISCON;
        }
    }

    // 8. Traffic Rates Calculation
    raw_net_stat_t curr_net;
    read_net_bytes(t->active_ifname, &curr_net);

    static const char *units[] = {"B/s", "K/s", "M/s", "G/s"};
    unsigned long long rx_diff = (curr_net.rx_bytes >= s_prev_net.rx_bytes) ? (curr_net.rx_bytes - s_prev_net.rx_bytes) : 0;
    unsigned long long tx_diff = (curr_net.tx_bytes >= s_prev_net.tx_bytes) ? (curr_net.tx_bytes - s_prev_net.tx_bytes) : 0;
    s_prev_net = curr_net;

    double rx_bps = (double)rx_diff / dt;
    double tx_bps = (double)tx_diff / dt;

    int rx_u = 0, tx_u = 0;
    while (rx_bps >= 1000.0 && rx_u < 3) { rx_bps /= 1000.0; rx_u++; }
    while (tx_bps >= 1000.0 && tx_u < 3) { tx_bps /= 1000.0; tx_u++; }

    t->rx_rate = (float)rx_bps;
    t->tx_rate = (float)tx_bps;
    t->rx_unit = units[rx_u];
    t->tx_unit = units[tx_u];
}

void collector_cleanup(void) {
    // Nothing dynamic to free
}
