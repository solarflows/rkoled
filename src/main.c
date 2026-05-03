#include <stdio.h>
#include <stdlib.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>
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

// ========== 辅助: 绘制带进度条的系统指标行 ==========
// 格式: "LABEL PP% ████████░░░░░░"
// rightReserve: 行右侧保留的像素数(用于放置温度等额外信息), CPU行传40, 其余传0
static void DrawMetricBar(uint8_t x, uint8_t y, const char* label,
                          int pct, uint8_t rightReserve) {
    char buf[16];
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%s %d%%", label, pct);
    SSD1306_PutString(x, y, buf, MF_6x8, White);

    // 进度条起点 = 文本末尾 + 3px 间距
    int barX = x + (int)strlen(buf) * 6 + 3;
    int barEnd = 127 - (int)rightReserve;  // 右边保留空间
    int barMaxW = barEnd - barX;
    if (barMaxW < 4) return;  // 空间太小不画
    int barW = barMaxW * pct / 100;
    if (barW < 0) barW = 0;
    if (barW > barMaxW) barW = barMaxW;

    // 空心外框
    SSD1306_DrawRectangle2(barX, y + 1, barMaxW, 5, White);
    // 实心填充
    if (barW > 0 && pct > 0) {
        SSD1306_FillRect2(barX + 1, y + 2, barW, 3, White);
    }
}

void Work()
{
#if BURNIN_PREVENTION
    // --- 防烧屏：硬件垂直像素偏移 ---
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

    // ========== 布局线 (减少至3条, 底边自然留白) ==========
    SSD1306_DrawLine(0, 0, 127, 0, White);     // 顶边
    SSD1306_DrawLine(0, 13, 127, 13, White);   // 网络行下
    SSD1306_DrawLine(0, 47, 127, 47, White);   // 网速行上

    // ========== 第1行: 网络图标 + IP + 时钟 (y=1, MF_7x10) ==========
    NetIfInfo* main_if = g_main_if;
    int phy_count = g_phy_count, vlan_count = g_vlan_count;

    if (vlan_count > 0) {
        SSD1306_DrawBitMap(0, 1, ETH_CONNECT, 12, 12, White);
        SSD1306_PutString(14, 2, "VLAN Mode", MF_7x10, White);
    } else if (phy_count > 1) {
        SSD1306_DrawBitMap(0, 1, ETH_CONNECT, 12, 12, White);
        SSD1306_PutString(14, 2, "MultiEth", MF_7x10, White);
        if (main_if) {
            memset(ipStr, 0, IP_STR_LEN);
            GetLocalIP(main_if->name, ipStr);
        }
    } else if (main_if) {
        if (strncmp(main_if->name, "wlan", 4) == 0) {
            SSD1306_DrawBitMap(0, 1, WIFI_CONNECT, 12, 12, White);
        } else {
            SSD1306_DrawBitMap(0, 1, ETH_CONNECT, 12, 12, White);
        }
        memset(ipStr, 0, IP_STR_LEN);
        if (GetLocalIP(main_if->name, ipStr) != 0)
            memset(ipStr, 0, IP_STR_LEN);
        // IP截断: MF_7x10=7px宽, 时钟占35px, 留8px间距 → IP最大 (128-14-35-8)/7≈10字符
        if (strlen(ipStr) > 10) ipStr[10] = '\0';
        SSD1306_PutString(14, 2, ipStr, MF_7x10, White);
    } else {
        SSD1306_DrawBitMap(0, 1, NET_ERROR, 12, 12, White);
        SSD1306_PutString(14, 2, "NoNet", MF_7x10, White);
    }

    // 时钟 (右上角, MF_7x10, "HH:MM"=5字符×7px=35px)
    time_t now2 = time(NULL);
    struct tm* t = localtime(&now2);
    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "%02d:%02d", t->tm_hour, t->tm_min);
    SSD1306_PutString(128 - 5 * 7 - 1, 2, tempStr, MF_7x10, White);

    // ========== 第2行: CPU + 温度 (y=15, MF_6x8, 8px) ==========
    int cpuPct = (int)(cpuUsage + 0.5f);
    if (cpuPct < 0) cpuPct = 0;
    if (cpuPct > 100) cpuPct = 100;
    DrawMetricBar(0, 15, "CPU", cpuPct, 40);

    // 温度 (CPU行右侧)
    float cpuTemp = GetCpuTemp();
    memset(tempStr, 0, TEMP_STR_LEN);
    if (cpuTemp > -100.0f) {
        sprintf(tempStr, "%.1fC", cpuTemp);
    } else {
        sprintf(tempStr, "N/A");
    }
    SSD1306_PutString(128 - (int)strlen(tempStr) * 6 - 1, 15, tempStr, MF_6x8, White);

    // ========== 第3行: 内存 (y=26, MF_6x8) ==========
    int memPct = GetMemUsage();
    if (memPct < 0) memPct = 0;
    if (memPct > 100) memPct = 100;
    DrawMetricBar(0, 26, "Mem", memPct, 0);

    // ========== 第4行: 磁盘 (y=37, MF_6x8) ==========
    float diskPct = GetDiskUsagePercentage();
    int diskPctInt = (int)(diskPct + 0.5f);
    if (diskPctInt < 0) diskPctInt = 0;
    if (diskPctInt > 100) diskPctInt = 100;
    DrawMetricBar(0, 37, "Disk", diskPctInt, 0);

    // ========== 第5行: 网络速度 (y=50, MF_6x8) ==========
    // 速率单位转换 (防数组越界: txUnitLevel/rxUnitLevel 上限=3)
    float tx = tx_rates, rx = rx_rates;
    uint8_t txUnit = 0, rxUnit = 0;
    while (tx >= 1000.0f && txUnit < 3) { tx /= 1000.0f; txUnit++; }
    while (rx >= 1000.0f && rxUnit < 3) { rx /= 1000.0f; rxUnit++; }

    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "Up:%.1f%s", (double)tx, netSpeedUnit[txUnit]);
    SSD1306_PutString(5, 50, tempStr, MF_6x8, White);

    memset(tempStr, 0, TEMP_STR_LEN);
    sprintf(tempStr, "Down:%.1f%s", (double)rx, netSpeedUnit[rxUnit]);
    SSD1306_PutString(68, 50, tempStr, MF_6x8, White);

    SSD1306_UpdateScreen();
}
