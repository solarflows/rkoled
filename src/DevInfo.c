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
float GetDiskUsagePercentage() {
    struct statfs diskInfo;
    if (statfs("/home", &diskInfo) == -1) {
        return 0.0; // Error
    }

    unsigned long long totalDisk = diskInfo.f_blocks * diskInfo.f_bsize;
    unsigned long long availableDisk = diskInfo.f_bavail * diskInfo.f_bsize;

    float usagePercentage = (1 - (float)availableDisk / totalDisk) * 100.0;

    return usagePercentage;
}
