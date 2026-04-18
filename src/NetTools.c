#include "NetTools.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <features.h>
#include <dirent.h>
#include <stdbool.h>
#include "UserCfg.h"

#define MAX_LEN             32
#define CON_STR             "up\n"
#define DIS_STR             "down\n"
#define BUFFER_LEN          1024

char tempBuf[MAX_LEN];
char buffer[BUFFER_LEN] = "";   //文件中的内容暂存在字符缓冲区里

int ReadContext(const char* filePath, char* buf, int bufLen);

// 新增：获取所有网卡信息
int GetAllNetIfs(NetIfInfo* ifs, int max_ifs, int* phy_count, int* vlan_count, int* up_count) {
    DIR* dir = opendir("/sys/class/net");
    if (!dir) {
        printf("[NetInfo] Failed to open /sys/class/net\n");
        return 0;
    }
    struct dirent* entry;
    int count = 0;
    *phy_count = 0;
    *vlan_count = 0;
    *up_count = 0;
    while ((entry = readdir(dir)) && count < max_ifs) {
        if (entry->d_name[0] == '.') continue;
        if (count >= max_ifs) {
            printf("[NetInfo] Too many interfaces, increase MAX_NETIFS\n");
            break;
        }
        strncpy(ifs[count].name, entry->d_name, sizeof(ifs[count].name)-1);
        ifs[count].name[sizeof(ifs[count].name)-1] = '\0';
        // VLAN判断：检查uevent文件内容
        char uevent_path[128];
        snprintf(uevent_path, sizeof(uevent_path), "/sys/class/net/%s/uevent", entry->d_name);
        FILE* uevent_fp = fopen(uevent_path, "r");
        ifs[count].is_vlan = false;
        if (uevent_fp) {
            char line[128];
            while (fgets(line, sizeof(line), uevent_fp)) {
                if (strstr(line, "DEVTYPE=vlan")) {
                    ifs[count].is_vlan = true;
                    break;
                }
            }
            fclose(uevent_fp);
        } else if (strchr(entry->d_name, '.') != NULL || strstr(entry->d_name, "vlan") == entry->d_name || strstr(entry->d_name, "vlan-") == entry->d_name) {
            ifs[count].is_vlan = true;
        }
        // 物理网卡判断：检查device目录
        ifs[count].is_physical = false;
        if (!ifs[count].is_vlan) {
            char device_path[128];
            snprintf(device_path, sizeof(device_path), "/sys/class/net/%s/device", entry->d_name);
            ifs[count].is_physical = (access(device_path, F_OK) == 0);
        }
        // 判断up状态
        char path[128];
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", entry->d_name);
        FILE* fp = fopen(path, "r");
        if (fp) {
            char state[16] = "";
            if (!fgets(state, sizeof(state), fp)) {
                printf("[NetInfo] Failed to read operstate for %s\n", entry->d_name);
                state[0] = '\0';
            }
            fclose(fp);
            // 去除换行符
            char* nl = strchr(state, '\n');
            if (nl) *nl = '\0';
            ifs[count].is_up = (strcmp(state, "up") == 0);
        } else {
            printf("[NetInfo] Cannot open operstate for %s\n", entry->d_name);
            ifs[count].is_up = false;
        }
        if (ifs[count].is_physical) (*phy_count)++;
        if (ifs[count].is_vlan) (*vlan_count)++;
        if (ifs[count].is_up) (*up_count)++;
        count++;
    }
    closedir(dir);
    return count;
}

NetIfInfo* GetFirstActivePhysical(NetIfInfo* ifs, int count) {
    for (int i = 0; i < count; ++i) {
        if (ifs[i].is_physical && ifs[i].is_up) return &ifs[i];
    }
    return NULL;
}

Net_State GetWirelessState()
{
    char state[MAX_LEN] = "";
    if (ReadContext(WIRELESS, state, MAX_LEN) == 0)
    {
        if (!strcmp(state, CON_STR))
        {
            return STATE_CONNECT;
        }
        else if (!strcmp(state, DIS_STR))
        {
            return STATE_DISCONNECT;
        }
        else
        {
            return STATE_FAIL;
        }
    }
    return STATE_FAIL;
}

Net_State GetEthernetState()
{
    char state[MAX_LEN] = "";
    if (ReadContext(ETHERNET, state, MAX_LEN) == 0)
    {
        if (!strcmp(state, CON_STR))
        {
            return STATE_CONNECT;
        }
        else if (!strcmp(state, DIS_STR))
        {
            return STATE_DISCONNECT;
        }
        else
        {
            return STATE_FAIL;
        }
    }

    return STATE_FAIL;
}

Net_State GetNetState()
{
    if (GetWirelessState() == STATE_CONNECT || GetEthernetState() == STATE_CONNECT)
    {
        return STATE_CONNECT;
    }
    else
    {
        return STATE_DISCONNECT;
    }
}

int ReadContext(const char* filePath, char* buf, int bufLen)
{
    FILE* fp;
    fp = fopen(filePath, "r");
    if (fp == NULL)
    {
#ifdef ENABLE_LOG
        printf("Can't Open File \"%s\"\n", filePath);  
#endif //ENABLE_LOG
        return -1;
    }
    else
    {
        memset(tempBuf, 0, MAX_LEN);
        fread(tempBuf, 1, MAX_LEN, fp);

        if (strlen(tempBuf) < bufLen)
        {
            memcpy(buf, tempBuf, strlen(tempBuf));
        }
        else
        {
#ifdef ENABLE_LOG
            printf("Dst Buffer Too Small\n");
#endif //ENABLE_LOG
            return -1;
        }
        fclose(fp);
    }
    
    return 0;
}

int GetLocalIP(char * ifname, char * ip)
{
    char *temp = NULL;
    int inet_sock;
    struct ifreq ifr;

    if (!ifname || !ip) {
        printf("[NetInfo] GetLocalIP: invalid arguments\n");
        return -1;
    }

    inet_sock = socket(AF_INET, SOCK_DGRAM, 0); 
    if (inet_sock < 0) {
        printf("[NetInfo] GetLocalIP: failed to create socket\n");
        return -1;
    }

    memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);

    if (ioctl(inet_sock, SIOCGIFADDR, &ifr) != 0) 
    {   
        printf("[NetInfo] ioctl error for %s\n", ifname);
        close(inet_sock);
        return -1;
    }

    temp = inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr);     
    strncpy(ip, temp, 16);
    ip[15] = '\0';

    close(inet_sock);

    return 0;
}

int GetCurNetFlow(char* netname, long int* rx_rate, long int* tx_rate)
{
    FILE* fp = NULL;     //文件指针
    memset(buffer, 0, BUFFER_LEN);
    char* match = NULL;
    
    //错误参数检查
    if((NULL == netname) || (NULL == rx_rate) || (NULL == tx_rate))
    {
        printf("[NetInfo] GetCurNetFlow: bad param\n");
        return -1;    
    }
    //文件打开失败检查
    if ((fp = fopen("/proc/net/dev", "r")) == NULL)
    {
        printf("[NetInfo] Can't Open File /proc/net/dev/\n");
        return -1;
    }
    //读文件
    if (fread(buffer, 1, BUFFER_LEN, fp) < 200)
    {
        printf("[NetInfo] /proc/net/dev read too short\n");
        fclose(fp);
        return -1;
    }
    else
    {
        fclose(fp);
        match = strstr(buffer, netname);
        
        if (match == NULL)
        {
            printf("[NetInfo] Net interface %s not found in /proc/net/dev\n", netname);
            return -1;
        }
        else
        {
            match += (strlen(netname) + 1);
            sscanf(match, "%ld %*d %*d %*d %*d %*d %*d %*d %ld", rx_rate, tx_rate);
        }
    }
    return 0;/*返回成功*/
}
