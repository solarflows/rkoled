# rkoled 2.0 - 通用超低耗 Linux OLED (SSD1306 / SH1106) 系统监视守护进程

> **专为嵌入式 Linux 单板电脑 (Panther-X2, 树莓派, 香橙派, 友善, x86 等) 打造的轻量级硬件监控守护进程**
> 脱离庞大的 wiringPi 依赖，纯原生 Linux I2C ioctl 通信，CPU 时间由原版大幅缩减至 `< 0.03%`，支持多 UI 风格自由切换与丰富 CLI 参数。

---

## 一、 核心特性与架构升级

1. **通用化与零重依赖 (Universal & Zero-Bloat)**：
   - 彻底废除 `wiringPi` / `wiringOP` 的 200KB+ 杂乱源码依赖，直接基于 Linux 内核标准 `/dev/i2c-*` 设备节点通信；
   - 纯标准 POSIX C99 编写，无需外部库，可一键交叉编译至 `arm64`, `armhf`, `x86_64`, `riscv64` 等任何架构；
   - 支持 SSD1306 / SSD1315 与 SH1106（自动处理 132 列 2 像素偏移）。

2. **极限低耗 (Ultra-Low Overhead)**：
   - 连续采样算法替代原版粗暴的内部 `sleep(1)`，消除双重文件读取与 CPU 空转等待；
   - 采用 SSD1306 水平寻址流式批量写入，大幅缩减 I2C 系统调用上下文切换开销；
   - 单核运行 CPU 占用率稳定保持在 `< 0.03%` 极限水准。

3. **双套 UI 原生保留与参数化自由切换**：
   - **`astra` (默认推荐)**：高对比度工控胶囊风格。四大胶囊定宽 21px 严密对齐，右列 VLAN 与上下行多层排版绝对防溢出，底栏创新融入 **System Load 进度条背景与动态双色反相 IP 文字 (XOR Inversion)**；
   - **`classic` (原版经典)**：100% 逐像素还原原版四宫格布局与字符排版；
   - **开机彩蛋**：保留专属的「Panther X2」+「Husky」特大 16×26 点阵欢迎画面（可通过 `-n` 参数快速跳过）。

4. **高级网络与 VLAN 原生识别**：
   - 自动解析 `/proc/net/route` 智能绑定拥有默认网关的主管理口；
   - 原生支持 802.1Q VLAN 虚拟网卡（如 `end0.10`, `eth0.100`）；
   - 支持多 VLAN 4 秒平滑轮播（`-v cycle`）或固定单一 VLAN ID（`-v 10`）。

5. **底层字符集扩展**：
   - 内置单格点阵原生「`℃`」（Unicode `U+2103`）字模，彻底解决默认 ASCII 字符集丢弃度数符号的历史遗留问题。

---

## 二、 命令行参数体系 (CLI Reference)

```text
Usage: rkoled [OPTIONS]

Hardware Options:
  -d, --device <path>     I2C 设备节点路径 (默认: /dev/i2c-1)
  -a, --addr <hex>        I2C 从机地址 (默认: 0x3c)
  -t, --type <type>       屏幕驱动芯片型号: 'ssd1306' (默认), 'sh1106'
  -c, --contrast <0-255>  OLED 发光对比度 (默认: 127)

Display & UI Options:
  -s, --style <name>      UI 界面风格: 'astra' (默认), 'classic' (原版四宫格), 'lumen'
  -n, --no-boot           跳过启动时的 'Husky' 开机欢迎画面
  -r, --rate <sec>        遥测数据刷新步进秒数 (默认: 1)
  -T, --terminal          在终端控制台实时镜像点阵屏幕 (人不在设备前也可实时监控)
  -P, --dump              单次抓取屏幕 128x64 点阵快照打印至终端后退出 (远程调试神技)
  -X, --dry-run           无物理 I2C 硬件模拟运行模式

Network & Storage Options:
  -i, --iface <name>      手动指定监控网卡接口 (默认: 自动发现默认路由主网卡)
  -v, --vlan <mode|id>    VLAN 模式: 'auto' (主路由网段), 'cycle' (多VLAN自动轮换), 或固定ID (如: 10)
  -m, --mount <path>      监控磁盘占用的挂载点路径 (默认: /opt，自动回退到 /)

Daemon Options:
  -D, --daemon            作为系统后台守护进程运行 (自动 fork/setsid)
  -V, --version           打印程序版本信息并退出
  -h, --help              打印帮助信息并退出
```

---

## 三、 快速上手与编译

### 1. 本机编译
```bash
cd rkoled
make clean
make
```
编译产物为 `rkoled`，同时自动创建软链接 `RTDevInfo` 以完全向后兼容旧版服务脚本。

### 2. 命令行运行测试
```bash
# 默认模式启动 (Astra 风格, /dev/i2c-1, 自动识别主网卡)
sudo ./rkoled

# 喜欢原版经典四宫格 UI
sudo ./rkoled -s classic

# 指定以太网或 VLAN 虚拟网卡
sudo ./rkoled -i end0.10

# 开启多 VLAN 自动轮播模式
sudo ./rkoled -v cycle

# 指定 I2C 总线节点与屏幕对比度
sudo ./rkoled -d /dev/i2c-3 -c 200
```

---

## 四、 systemd 开机自启配置

新建或更新服务文件 `/etc/systemd/system/rkoled.service`：

```ini
[Unit]
Description=rkoled OLED System Monitor Daemon
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/bin/rkoled -d /dev/i2c-1 -s astra
Restart=on-failure
RestartSec=5s
KillMode=process

[Install]
WantedBy=multi-user.target
```

激活并启动服务：
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now rkoled
```
