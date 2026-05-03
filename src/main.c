#include <stdio.h>
#include <stdlib.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <string.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "SSD1306_IIC.h"
#include "DevInfo.h"
#include "NetTools.h"
#include "UserCfg.h"

#define TEMP_STR_LEN    64
#define IP_STR_LEN      20
#define TO_GB(b)        ((b) / pow(1024, 3))
#define TO_SEC(h, m)    ((h) * 3600 + (m) * 60)

extern uint8_t WIFI_CONNECT[32];
extern uint8_t ETH_CONNECT[32];
extern uint8_t NET_ERROR[32];

time_t timep;
struct tm* myTm;
uint8_t inited = 1;

#if ENABLE_RUNNING_PERIOD
int BEG_SEC = TO_SEC(BEG_H, BEG_M);
int END_SEC = TO_SEC(END_H, END_M);
int CUR_SEC = 0;
#endif // ENABLE_RUNNING_PERIOD

long int start_rcv_rates = 0;   //保存开始时的流量计数
long int end_rcv_rates = 0;	    //保存结束时的流量计数
long int start_tx_rates = 0;    //保存开始时的流量计数
long int end_tx_rates = 0;
char ipStr[IP_STR_LEN];
char tempStr[TEMP_STR_LEN];
float tx_rates = 0;
float rx_rates = 0;
float cpuUsage;
CPU_OCCUPY cpu_stat1;
CPU_OCCUPY cpu_stat2;
// Net_State wlanState;
// Net_State ethState; // 已废弃，防止未使用警告
char netSpeedUnit[4][5] = {
    "B/s",
    "KB/s",
    "MB/s",
    "GB/s",
};

void Work();
void CacheNetIfInfo();

// 前向声明
static NetIfInfo* g_main_if;

int main(int argc, char* argv[])
{
    SSD1306_Init();

    // 启动时缓存网卡信息
    CacheNetIfInfo();

    if (!(argc >= 2 && !strcmp(argv[1], "-r")))
    {
        SSD1306_FillRect2(2, 0, 5, 128, White);
        SSD1306_PutString(21, 2, "Panther X2", MF_6x8, White);
        SSD1306_PutString(15, 16, "Husky", MF_16x26, White);
        SSD1306_PutString(90, 25, "", MF_11x18, White);
        SSD1306_PutString(15, 52, "HomeAssistant", MF_6x8, White);
        SSD1306_UpdateScreen();
        sleep(3);
    }

    time_t last_cache_update = 0;
    while (1)
    {
        // 每60秒自动刷新一次网卡信息缓存
        time_t now = time(NULL);
        if (now - last_cache_update >= 60) {
            CacheNetIfInfo();
            last_cache_update = now;
        }
#if ENABLE_RUNNING_PERIOD
        time(&timep);
        myTm = localtime(&timep);
        CUR_SEC = TO_SEC(myTm->tm_hour, myTm->tm_min);

        if (BEG_SEC <= CUR_SEC && CUR_SEC <= END_SEC)
        {
            inited = 1;
        }
        else if (inited)
        {
            inited = 0;
            SSD1306_ClearScreen();
            SSD1306_UpdateScreen();
        }

        if (inited)
        {
#endif // ENABLE_RUNNING_PERIOD
            Work();
            if (g_main_if)
                GetCurNetFlow(g_main_if->name, &start_rcv_rates, &start_tx_rates);
            get_cpuoccupy((CPU_OCCUPY *)&cpu_stat1);
            sleep(REFRESH_TIME);
            get_cpuoccupy((CPU_OCCUPY *)&cpu_stat2);
            if (g_main_if)
                GetCurNetFlow(g_main_if->name, &end_rcv_rates, &end_tx_rates);

            cpuUsage = cal_cpuoccupy((CPU_OCCUPY *)&cpu_stat1, (CPU_OCCUPY *)&cpu_stat2);
            rx_rates = (float)(end_rcv_rates - start_rcv_rates) / REFRESH_TIME;
            tx_rates = (float)(end_tx_rates - start_tx_rates) / REFRESH_TIME;

#if ENABLE_RUNNING_PERIOD
        }
        else
        {
            sleep(10);
        }
#endif // ENABLE_RUNNING_PERIOD

    }
}

// 全局缓存变量
#define MAX_NETIFS 16
static NetIfInfo g_ifs[MAX_NETIFS];
static int g_phy_count = 0, g_vlan_count = 0, g_up_count = 0, g_if_count = 0;
// static NetIfInfo* g_main_if = NULL; // 已经在上面声明了
static int g_cache_valid = 0; // 0:无效 1:有效

void CacheNetIfInfo() {
    int last_if_count = g_if_count;
    NetIfInfo last_ifs[MAX_NETIFS];
    memcpy(last_ifs, g_ifs, sizeof(g_ifs));
    int ret = GetAllNetIfs(g_ifs, MAX_NETIFS, &g_phy_count, &g_vlan_count, &g_up_count);
    if (ret > 0) {
        g_if_count = ret;
        g_main_if = GetFirstActivePhysical(g_ifs, g_if_count);
        g_cache_valid = 1;
        printf("[NetInfo] Cache updated: total=%d, phy=%d, vlan=%d, up=%d\n", g_if_count, g_phy_count, g_vlan_count, g_up_count);
    } else {
        printf("[NetInfo] Cache update failed, keep last valid cache\n");
        memcpy(g_ifs, last_ifs, sizeof(g_ifs));
        g_if_count = last_if_count;
        g_main_if = GetFirstActivePhysical(g_ifs, g_if_count);
        // g_cache_valid 不变
    }
}

void Work()
{
#if BURNIN_PREVENTION
    // --- 防烧屏：硬件垂直像素偏移 ---
    // 利用 SSD1306 Display Offset (0xD3) 在 0~2 像素间来回偏移，
    // 使静态内容(分隔线、标签)不会始终点亮同一行像素，有效延缓烧屏。
    static time_t last_shift_time = 0;
    static uint8_t display_offset = 0;
    static int shift_dir = 1;

    time_t now = time(NULL);
    if (now - last_shift_time >= SHIFT_INTERVAL_SEC) {
        display_offset = (display_offset + shift_dir) & 0x3F;
        if (display_offset >= 2) shift_dir = -1;
        if (display_offset == 0) shift_dir = 1;
        last_shift_time = now;
    }
    SSD1306_SetDisplayOffset(display_offset);

#if NIGHT_CONTRAST_ENABLE
    // --- 夜间低亮度模式 ---
    // 夜间降低 OLED 对比度，既防烧屏又减少光污染
    struct tm* tm_info = localtime(&now);
    int hour = tm_info->tm_hour;
    static int last_night_state = -1;
    int is_night = (hour >= NIGHT_BEG_H && hour < NIGHT_END_H) ? 1 : 0;
    if (is_night != last_night_state) {
        SSD1306_SetContrast(is_night ? NIGHT_CONTRAST : 0x7F);
        last_night_state = is_night;
    }
#endif
#endif // BURNIN_PREVENTION

    SSD1306_ClearScreen();

    SSD1306_DrawLine(0, 0, 127, 0, White);
    SSD1306_DrawLine(0, 15, 127, 15, White);
    SSD1306_DrawLine(82, 15, 82, 38, White);
    SSD1306_DrawLine(0, 38, 127, 38, White);
    SSD1306_DrawLine(0, 63, 127, 63, White);
    SSD1306_DrawLine(37, 39, 37, 62, White);

    // 使用缓存的网卡信息
    NetIfInfo* ifs = g_ifs;
    int phy_count = g_phy_count, vlan_count = g_vlan_count, up_count = g_up_count, if_count = g_if_count;
    NetIfInfo* main_if = g_main_if;
    // 调试输出
    // printf("[NetInfo] phy=%d vlan=%d up=%d if_count=%d\n", phy_count, vlan_count, up_count, if_count);
    (void)ifs; // 防止未使用警告

    // VLAN优先显示：只要有VLAN接口就显示VLAN Mode
    if (vlan_count > 0) {
        SSD1306_DrawBitMap(0, 0, ETH_CONNECT, 16, 16, White);
        SSD1306_PutString(17, 4, "VLAN Mode", MF_7x10, White);
    } else if (phy_count > 1) {
        SSD1306_DrawBitMap(0, 0, ETH_CONNECT, 16, 16, White);
        SSD1306_PutString(17, 4, "MultiEth", MF_7x10, White);
        if (main_if) {
            memset(ipStr, 0, IP_STR_LEN);
            if (GetLocalIP(main_if->name, ipStr) != 0)
                memset(ipStr, 0, IP_STR_LEN);
        }
    } else if (main_if) {
        if (strncmp(main_if->name, "wlan", 4) == 0) {
            SSD1306_DrawBitMap(0, 0, WIFI_CONNECT, 16, 16, White);
        } else {
            SSD1306_DrawBitMap(0, 0, ETH_CONNECT, 16, 16, White);
        }
        memset(ipStr, 0, IP_STR_LEN);
        if (GetLocalIP(main_if->name, ipStr) != 0)
            memset(ipStr, 0, IP_STR_LEN);
        SSD1306_PutString(17, 4, ipStr, MF_7x10, White);
    } else {
        SSD1306_DrawBitMap(0, 0, NET_ERROR, 16, 16, White);
        SSD1306_PutString(17, 4, "NoNet", MF_7x10, White);
    }

    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "CPU: %.1f%%", cpuUsage);
    SSD1306_PutString(0, 17, tempStr, MF_7x10, White);

    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "Mem: %d%%", GetMemUsage());
    SSD1306_PutString(0, 28, tempStr, MF_7x10, White);

    SSD1306_PutString(91, 17, "Temp", MF_7x10, White);
    sprintf(tempStr, "%.1fC", GetCpuTemp());
    SSD1306_PutString(88, 28, tempStr, MF_7x10, White);

    uint8_t rxUnitLevel = 0;
    uint8_t txUnitLevel = 0;
    while (tx_rates >= 1000)
    {
        tx_rates /= 1000;
        txUnitLevel++;
    }
    while (rx_rates >= 1000)
    {
        rx_rates /= 1000;
        rxUnitLevel++;
    }

    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, " Up :%.1lf %s",
            tx_rates,
            netSpeedUnit[txUnitLevel]);
    SSD1306_PutString(40, 42, tempStr, MF_6x8, White);

    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "Down:%.1lf %s",
            rx_rates,
            netSpeedUnit[rxUnitLevel]);
    SSD1306_PutString(40, 52, tempStr, MF_6x8, White);

    SSD1306_PutString(7, 42, "Disk", MF_6x8, White);
    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "%.1f%%", GetDiskUsagePercentage());
    SSD1306_PutString(4, 52, tempStr, MF_6x8, White);

    SSD1306_UpdateScreen();
}
