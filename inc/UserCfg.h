/*无线网卡文件名*/
#define WLAN_IF         "wlan0"
/*有线网卡文件名*/
#define ETH_IF          "end0"
/*i2c设备文件名*/
#define LINUX_IIC_FILE  "/dev/i2c-1"
/*刷新时间(s)*/
#define REFRESH_TIME    1

/*是否启用运行时间控制: 若启用，屏幕只会在固定时间点亮*/
#define ENABLE_RUNNING_PERIOD   0

#if ENABLE_RUNNING_PERIOD
#define BEG_H           0   //开始运行时
#define BEG_M           00  //开始运行分
#define END_H           23  //结束运行时
#define END_M           59  //结束运行分
#endif //ENABLE_RUNNING_PERIOD

/* ========== OLED 防烧屏配置 ========== */
/* 启用防烧屏：1=启用, 0=禁用 */
#define BURNIN_PREVENTION           1
/* 像素偏移间隔(秒)：每隔多久垂直偏移1像素 */
#define SHIFT_INTERVAL_SEC          120
/* 夜间低对比度：1=启用, 0=禁用 */
#define NIGHT_CONTRAST_ENABLE       1
/* 夜间对比度值 (0x00最暗 ~ 0xFF最亮, 默认0x7F) */
#define NIGHT_CONTRAST              0x2F
/* 夜间模式开始-时(0-23) */
#define NIGHT_BEG_H                 0
/* 夜间模式结束-时(0-23) */
#define NIGHT_END_H                 6

#define WIRELESS    "/sys/class/net/"WLAN_IF"/operstate"
#define ETHERNET    "/sys/class/net/"ETH_IF"/operstate"
