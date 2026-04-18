#ifndef NET_TOOLS_H
#define NET_TOOLS_H

#define ENABLE_LOG

typedef enum
{
    STATE_CONNECT = 0,
    STATE_DISCONNECT,
    STATE_FAIL,
}Net_State;

// 新增结构体用于网卡信息
#include <stdbool.h>
typedef struct {
    char name[32];
    bool is_vlan;
    bool is_physical;
    bool is_up;
} NetIfInfo;

int GetAllNetIfs(NetIfInfo* ifs, int max_ifs, int* phy_count, int* vlan_count, int* up_count);
NetIfInfo* GetFirstActivePhysical(NetIfInfo* ifs, int count);

Net_State GetWirelessState();
Net_State GetEthernetState();
Net_State GetNetState();
int GetLocalIP(char * ifname, char * ip);
int GetCurNetFlow(char* netname, long int* rx_rate, long int* tx_rate);

#endif //NET_TOOLS_H
