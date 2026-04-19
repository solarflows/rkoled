# rkoled

> 本项目基于 [Temperature6/OPi4_RTDevInfo](https://github.com/Temperature6/OPi4_RTDevInfo) 二次开发，感谢原作者。

## 交叉编译与 Buildroot 支持

本项目已适配 Buildroot 交叉编译环境。编译时请使用 Buildroot 提供的交叉工具链：

```sh
make CC=aarch64-buildroot-linux-gnu-gcc CFLAGS="-O2" LDFLAGS=""
```
- `CC`、`CFLAGS`、`LDFLAGS` 可由 Buildroot 自动传入。
- Makefile 支持 out-of-tree 构建。

## 依赖说明
- 依赖 Rockchip/OrangePi 的 WiringOP（不是标准 wiringPi）。
- 项目已内置所有必要依赖（包括 WiringOP 相关头文件和源码），无需单独引入或安装。

## 使用方法（以香橙派zero3为例）

**0）硬件准备**

​	1.一块Linux开发板，安装了香橙派的wiringOP或者树莓派的wiringPi

​	2.一个使用四线I2C通信的SSD1306驱动芯片的128*64像素OLED小屏幕

​	3.杜邦线若干，或使用本仓库提供的小转接板（在Hardware目录下，是一个立创EDA工程）

**1）检查一下I2C外设的位置**

```shell
user@orangepizero3:~$ gpio readall
 +------+-----+----------+------+---+   H616   +---+------+----------+-----+------+
 | GPIO | wPi |   Name   | Mode | V | Physical | V | Mode | Name     | wPi | GPIO |
 +------+-----+----------+------+---+----++----+---+------+----------+-----+------+
 |      |     |     3.3V |      |   |  1 || 2  |   |      | 5V       |     |      |
 |  229 |   0 |    SDA.3 | ALT5 | 0 |  3 || 4  |   |      | 5V       |     |      |
 |  228 |   1 |    SCL.3 | ALT5 | 0 |  5 || 6  |   |      | GND      |     |      |
 |   73 |   2 |      PC9 |  OFF | 0 |  7 || 8  | 0 | OFF  | TXD.5    | 3   | 226  |
 |      |     |      GND |      |   |  9 || 10 | 0 | OFF  | RXD.5    | 4   | 227  |
 |   70 |   5 |      PC6 | ALT5 | 0 | 11 || 12 | 0 | OFF  | PC11     | 6   | 75   |
 |   69 |   7 |      PC5 |  OUT | 1 | 13 || 14 |   |      | GND      |     |      |
 |   72 |   8 |      PC8 |  OUT | 1 | 15 || 16 | 0 | OFF  | PC15     | 9   | 79   |
 |      |     |     3.3V |      |   | 17 || 18 | 0 | OFF  | PC14     | 10  | 78   |
 |  231 |  11 |   MOSI.1 | ALT4 | 0 | 19 || 20 |   |      | GND      |     |      |
 |  232 |  12 |   MISO.1 | ALT4 | 0 | 21 || 22 | 1 | OUT  | PC7      | 13  | 71   |
 |  230 |  14 |   SCLK.1 | ALT4 | 0 | 23 || 24 | 0 | ALT4 | CE.1     | 15  | 233  |
 |      |     |      GND |      |   | 25 || 26 | 1 | OUT  | PC10     | 16  | 74   |
 |   65 |  17 |      PC1 |  OFF | 0 | 27 || 28 |   |      |          |     |      |
 |  272 |  18 |     PI16 | ALT2 | 0 | 29 || 30 |   |      |          |     |      |
 |  262 |  19 |      PI6 |  OFF | 0 | 31 || 32 |   |      |          |     |      |
 |  234 |  20 |     PH10 | ALT3 | 0 | 33 || 34 |   |      |          |     |      |
 +------+-----+----------+------+---+----++----+---+------+----------+-----+------+
 | GPIO | wPi |   Name   | Mode | V | Physical | V | Mode | Name     | wPi | GPIO |
 +------+-----+----------+------+---+   H616   +---+------+----------+-----+------+

```

**2）将四线I2C的OLED小屏幕连接到某个I2C外设上，例如香橙派Zero3的I2C.3**

> OLED   香橙派
>
> VCC    -   3.3V
>
> GND   -   GND
>
> SDA   -   SDA.3
>
> SCL    -   SCL.3

**3）检查设备是否打开 I2C.3**

```shell
user@orangepizero3:~$ ls /dev/i2c*
/dev/i2c-3  /dev/i2c-4  /dev/i2c-5
```

如果有/dev/i2c-3，说明已经开启

**4）检查屏幕是否正常连接**

安装i2c-tools，如果已安装可以跳过

```shell
sudo apt-get update
sudo apt-get install -y i2c-tools
```

屏幕连接的是i2c-3，所以检查i2c-3下面是否有设备

```shell
user@orangepizero3:~$ sudo i2cdetect -y 3
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
```

出现 3c 说明连接正常

**5）克隆本仓库**

```shell
git clone https://github.com/solarflows/rkoled.git
```

**6）进入文件夹**

```shell
cd rkoled/
```

**7）编辑i2c配置**

一般来说，只需要更改i2c配置即可，其他由用户自行决定

```shell
vim inc/UserCfg.h
```

文件内容如下

```C
/*无线网卡文件名*/
#define WLAN_IF         "wlan0"
/*有线网卡文件名*/
#define ETH_IF          "eth0"
/*i2c设备文件名*/
#define LINUX_IIC_FILE  "/dev/i2c-3"
/*刷新时间(s)*/
#define REFRESH_TIME    1

/*是否启用运行时间控制: 若启用，屏幕只会在固定时间点亮*/
#define ENABLE_RUNNING_PERIOD   0

#if ENABLE_RUNNING_PERIOD
#define BEG_H           7   //开始运行时
#define BEG_M           00  //开始运行分
#define END_H           23  //结束运行时
#define END_M           00  //结束运行分
#endif //ENABLE_RUNNING_PERIOD

#define WIRELESS    "/sys/class/net/"WLAN_IF"/operstate"
#define ETHERNET    "/sys/class/net/"ETH_IF"/operstate"

```

将

```
#define LINUX_IIC_FILE  "/dev/i2c-3"
```

改为需要的文件地址，由于本次连接的正好也是i2c3，不需要更改

如果需要更改其他值，更改完成后退出编辑器即可

**8）构建**

```shell
make
```

如果没有报错，此时ls一下就可以看到编译出来的文件

```shell
user@orangepizero3:~/rkold$ ls
inc  Makefile  obj  RTDevInfo  src
```

RTDevInfo就是可执行文件，可以运行测试，程序设计硬件操作，需要使用sudo权限

```shell
sudo ./RTDevInfo
```

没用问题的话，屏幕会成功点亮，程序默认有个开机画面，如果不需要，可以在main.c中删去

**9） 添加systemd开机自启**

```shell
# 新建systemd文件
nano /etc/systemd/system/SSD1315.service
# 将以下内容粘贴,需要自己修改,执行文件路径
[Unit]
Description=SSD1315 Service
StandardOutput=null
StandardError=null

[Service]
ExecStart=/自己的路径/RTDevInfo

[Install]
WantedBy=multi-user.target
# ctrl+x保存,y回车
# 接着启动服务
systemctl start SSD1315
# 设置开机自启
systemctl enable SSD1315
```