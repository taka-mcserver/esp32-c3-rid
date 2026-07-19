# ESP32-C3 RID Scanner + Simulator

基于 [luolitao/esp32-crid](https://github.com/luolitao/esp32-crid) 项目，为 ESP32-C3 SuperMini 开发板定制的合并固件。

## 功能

- **C-RID 嗅探与解码** — 接收并解码周围所有无人机 Remote ID 广播信号（支持 GB 42590-2023 / ASTM F3411-22a）
- **NVS 缓存** — 自动将接收到的无人机数据缓存到 Flash，直至存储空间用尽
- **C-RID 模拟发送** — 模拟无人机发送 RID 信息，让周围接收设备可以接收和解码
- **WiFi AP + Captive Portal** — 开发板发出名为 `rid` 的 WiFi，密码 `12345678`，手机连接后自动跳转到管理后台
- **Web 管理后台** — 仪表盘、无人机列表、详情页（含高德地图一键导航）、模拟器参数配置
- **串口调试输出** — UART1 (GPIO4 TX) 输出 JSON 格式调试信息，波特率 115200

## 硬件连接

| 功能 | GPIO |
|------|------|
| USB 串口 (内置) | GPIO18/19 (USB_D-/D+) |
| 调试串口 TX | GPIO4 |
| 调试串口 RX | GPIO5 |
| 调试串口 波特率 | 115200 |

## 快速开始

### 前置条件

安装 ESP-IDF v5.2.1 或更高版本：
```bash
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c3
. ./export.sh
```

### 编译

```bash
cd esp32-rid
idf.py set-target esp32c3
idf.py build
```

### 烧录

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

(Windows 上使用 COM 端口，如 `idf.py -p COM3 flash monitor`)

### 合并固件（用于量产烧录）

```bash
esptool.py --chip esp32c3 merge_bin \
  -o esp32-c3-rid-combined.bin \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x0 build/bootloader/bootloader.bin \
  0x10000 build/esp32_crid.bin \
  0x8000 build/partition_table/partition-table.bin
```

## 使用说明

1. 开发板上电后，会发出 WiFi 热点：
   - **SSID**: `rid`
   - **密码**: `12345678`

2. 手机连接该 WiFi 后，浏览器会自动跳转到管理后台（Captive Portal）

3. 管理后台功能：
   - **仪表盘**: 查看在线无人机数量、总接收数、模拟器状态
   - **无人机列表**: 查看所有已发现的无人机，点击查看详情
   - **无人机详情**: 包含位置信息，可一键跳转到高德地图导航
   - **模拟器**: 配置并控制模拟无人机发送参数

4. 串口调试：将 USB-TTL 模块连接到 GPIO4(TX) + GPIO5(RX) + GND，波特率 115200

## 项目结构

```
esp32-rid/
├── CMakeLists.txt          # 根构建文件
├── sdkconfig.defaults      # ESP32-C3 默认配置
├── main/                   # 主固件代码
│   ├── app_main.c          # 入口 + WiFi AP 初始化
│   ├── crid_sniffer.c      # WiFi 嗅探器
│   ├── crid_parser*.c      # RID 协议解析器
│   ├── crid_tracker.c      # 无人机追踪表
│   ├── crid_web.c          # Web 服务器 + Captive Portal
│   ├── crid_nvs.c          # NVS 缓存存储
│   ├── crid_serial.c       # 串口调试输出
│   ├── crid_config.c       # 模拟器配置
│   ├── crid_messages.c     # RID 报文构建
│   ├── crid_wifi.c         # WiFi 发送
│   ├── crid_patrol.c       # 巡游模拟
│   └── web/index.html      # Web UI
├── components/opendroneid/ # OpenDroneID 解码库
└── partition_table/        # 分区表
```

## API 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | /api/status | 系统状态（在线数、总数、内存） |
| GET | /api/drones | 无人机列表 |
| GET | /api/sim_config | 获取模拟器配置 |
| POST | /api/sim_config | 保存模拟器配置 |
| POST | /api/sim_toggle | 开关模拟器 |
| POST | /api/clear_cache | 清除缓存 |

## 固件下载

在 GitHub Actions 中自动构建，可在 [Releases](../../releases) 页面下载最新固件。

## 许可

基于原项目 [luolitao/esp32-crid](https://github.com/luolitao/esp32-crid) 修改。
