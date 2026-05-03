// DevInfo.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include "DevInfo.h"

#define TEMP_FILE_PATH  "/sys/class/thermal/thermal_zone0/temp"
#define MAX_SIZE        32

// Function to calculate CPU usage percentage
float cal_cpuoccupy(CPU_OCCUPY *o, CPU_OCCUPY *n) {
    unsigned long od, nd;
    unsigned long id, sd;
    float cpu_use = 0.0;

    od = (unsigned long)(o->user + o->nice + o->system + o->idle);
    nd = (unsigned long)(n->user + n->nice + n->system + n->idle);
    id = (unsigned long)(n->user - o->user);
    sd = (unsigned long)(n->system - o->system);

    if ((nd - od) != 0)
        cpu_use = ((sd + id) * 100.0) / (nd - od);
    else
        cpu_use = 0.0;

    return cpu_use;
}

// Function to get CPU occupy information
void get_cpuoccupy(CPU_OCCUPY *cpust) {
    FILE *fd;
    char buff[256];

    fd = fopen("/proc/stat", "r");
    if (fd != NULL) {
        fgets(buff, sizeof(buff), fd);
        sscanf(buff, "%s %u %u %u %u",
               cpust->name, &cpust->user, &cpust->nice,
               &cpust->system, &cpust->idle);
        fclose(fd);
    }
}

// Function to calculate CPU usage
float GetCpuUsage() {
    CPU_OCCUPY cpu_stat1, cpu_stat2;

    get_cpuoccupy(&cpu_stat1);
    sleep(1);
    get_cpuoccupy(&cpu_stat2);

    return cal_cpuoccupy(&cpu_stat1, &cpu_stat2);
}

// Function to get CPU temperature
float GetCpuTemp() {
    FILE *fp;
    char buf[MAX_SIZE];
    double tempVal = 0.0;

    fp = fopen(TEMP_FILE_PATH, "r");
    if (fp != NULL) {
        fread(buf, 1, MAX_SIZE, fp);
        tempVal = atof(buf) / 1000.0;
        fclose(fp);
    }

    return tempVal;
}

// Function to get system memory usage percentage
int GetMemUsage() {
    FILE* fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1; // Error opening file
    }

    long mem_total_kb = 0;
    long mem_free_kb = 0;
    long buffers_kb = 0;
    long cached_kb = 0;
    char buf[256];

    while (fgets(buf, sizeof(buf), fp)) {
        if (strncmp(buf, "MemTotal:", 9) == 0) {
            sscanf(buf + 9, "%ld", &mem_total_kb);
        } else if (strncmp(buf, "MemFree:", 8) == 0) {
            sscanf(buf + 8, "%ld", &mem_free_kb);
        } else if (strncmp(buf, "Buffers:", 8) == 0) {
            sscanf(buf + 8, "%ld", &buffers_kb);
        } else if (strncmp(buf, "Cached:", 7) == 0) {
            sscanf(buf + 7, "%ld", &cached_kb);
        }
    }

    fclose(fp);

    // Calculate used memory in kilobytes
    long used_mem_kb = mem_total_kb - mem_free_kb - buffers_kb - cached_kb;

    // Calculate memory usage percentage
    int mem_usage_percent = (int)((double)used_mem_kb / mem_total_kb * 100);

    return mem_usage_percent;
}

// Function to get disk usage percentage
// Scans /proc/mounts to find the largest real (non-virtual) filesystem,
// which correctly handles overlayfs used in HAOS / Docker environments.
float GetDiskUsagePercentage() {
    struct statfs diskInfo;
    float bestUsage = 0.0;
    unsigned long long bestTotal = 0;

    // Strategy: parse /proc/mounts to find the largest real filesystem.
    // This avoids overlayfs, tmpfs, and other virtual FS that report
    // misleading free space (e.g., HAOS /home is overlayfs upper dir).
    FILE* fp = fopen("/proc/mounts", "r");
    if (fp != NULL) {
        char line[512];
        char device[128], mountPoint[128], fsType[64];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "%127s %127s %63s", device, mountPoint, fsType) == 3) {
                // Skip virtual, pseudo, and overlay filesystems
                if (strcmp(fsType, "overlay") == 0 ||
                    strcmp(fsType, "tmpfs") == 0 ||
                    strcmp(fsType, "devtmpfs") == 0 ||
                    strcmp(fsType, "sysfs") == 0 ||
                    strcmp(fsType, "proc") == 0 ||
                    strcmp(fsType, "squashfs") == 0 ||
                    strcmp(fsType, "cgroup") == 0 ||
                    strcmp(fsType, "cgroup2") == 0 ||
                    strcmp(fsType, "debugfs") == 0 ||
                    strcmp(fsType, "tracefs") == 0 ||
                    strcmp(fsType, "securityfs") == 0 ||
                    strcmp(fsType, "pstore") == 0 ||
                    strcmp(fsType, "configfs") == 0 ||
                    strcmp(fsType, "ramfs") == 0 ||
                    strcmp(fsType, "hugetlbfs") == 0 ||
                    strncmp(fsType, "fuse.", 5) == 0) {
                    continue;
                }
                // Found a real filesystem — check its size
                if (statfs(mountPoint, &diskInfo) == 0) {
                    unsigned long long total = (unsigned long long)diskInfo.f_blocks * diskInfo.f_bsize;
                    if (total > bestTotal) {
                        bestTotal = total;
                        unsigned long long avail = (unsigned long long)diskInfo.f_bavail * diskInfo.f_bsize;
                        if (total > 0) {
                            bestUsage = (1.0f - (float)avail / (float)total) * 100.0f;
                        }
                    }
                }
            }
        }
        fclose(fp);
        if (bestTotal > 0) return bestUsage;
    }

    // Fallback: try common data partition paths in order
    const char* paths[] = {"/mnt/data", "/data", "/home", "/", NULL};
    for (int i = 0; paths[i] != NULL; i++) {
        if (statfs(paths[i], &diskInfo) == 0) {
            unsigned long long total = (unsigned long long)diskInfo.f_blocks * diskInfo.f_bsize;
            // Only consider filesystems larger than 100 MB
            if (total > 100ULL * 1024 * 1024) {
                unsigned long long avail = (unsigned long long)diskInfo.f_bavail * diskInfo.f_bsize;
                return (1.0f - (float)avail / (float)total) * 100.0f;
            }
        }
    }

    return 0.0;
}
