# Embedded Network Monitor

A real-time network monitoring application for embedded Linux devices. Built in C99 with [LVGL 8.3](https://lvgl.io/) for a responsive, touch-driven dark-themed UI. Originally developed for the [Luckfox Pico 86 Smart Panel](https://www.luckfox.com/) and designed to run on a broad range of embedded platforms.

![Language](https://img.shields.io/badge/language-C99-green)
![UI](https://img.shields.io/badge/UI-LVGL%208.3-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Features

- **Status Dashboard** -- At-a-glance network health: router reachability, DNS functionality, WAN link quality (next-hop detection, jitter, packet loss), WiFi status, and infrastructure summary
- **Interface Dashboard** -- Live interface cards with sparkline charts, RX/TX rates, and connectivity indicators (USB interfaces filtered from view)
- **Interface Detail** -- Per-interface drill-down with traffic stats, peak rates, daily totals, error/drop counts, and WiFi info (SSID, signal, bitrate)
- **Traffic Charts** -- 24-hour bandwidth history at 1-minute resolution with interface selector
- **Alert System** -- Threshold-based alerts with hysteresis for interface down, high bandwidth, packet errors/drops, gateway unreachable, DNS failure, and high latency
- **Connection Tracking** -- Live TCP/UDP connection table parsed from `/proc/net/tcp` and `/proc/net/udp`
- **WiFi Management** -- Enable/disable WiFi, scan for networks, connect with WPA/WPA2 authentication, auto-connect on boot
- **Settings** -- Network configuration, WiFi settings, alert thresholds, infrastructure device management

## Screenshots

*The UI runs on embedded touchscreens with a dark theme optimized for at-a-glance monitoring.*

## Supported Hardware

| Platform | Status |
|----------|--------|
| Luckfox Pico 86 Smart Panel (RV1106, 720x720) | Tested |
| Other embedded Linux with framebuffer + touch | Planned |

## Architecture

```
+------------------------------------------+
|  UI Layer (LVGL 8.3)                     |
|  Status, Interfaces, Detail, Traffic,    |
|  Alerts, Connections, Settings           |
+------------------------------------------+
|  Core Layer                              |
|  Config, Alerts Engine, Ring Buffers,    |
|  Data Persistence, Logging, Formatters   |
+------------------------------------------+
|  Network Layer                           |
|  /proc parsers, ICMP Ping, DNS Probe,   |
|  ARP, WiFi, WAN Discovery, Connection   |
|  Tracking, WiFi Management              |
+------------------------------------------+
|  HAL Layer                               |
|  Framebuffer (/dev/fb0)                  |
|  Evdev Touch (/dev/input/event0)         |
+------------------------------------------+
```

**Key design decisions:**
- **Single-threaded** cooperative event loop using LVGL timers
- **All static allocation** -- zero calls to `malloc`, no heap fragmentation
- **~700 KB** stripped binary, **~3 MB** RSS at runtime
- Data collected from `/proc` and `ioctl` at 1s/5s/30s intervals
- WAN link quality measured via lightweight traceroute and multi-ping

## Building

### Prerequisites

A Linux host (tested on Raspberry Pi / aarch64) with:

```bash
sudo apt-get install -y cmake gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf sshpass
```

### Clone

```bash
git clone --recursive https://github.com/soren42/Embedded_Network_Monitor.git
cd Embedded_Network_Monitor
```

If you cloned without `--recursive`, initialize submodules:

```bash
git submodule update --init --recursive
```

### Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-rockchip.cmake
make -j$(nproc)
```

The resulting `embedded_netmon` binary is a statically-linked ARM ELF.

## Deployment

### Automated

```bash
./deploy/deploy.sh [target_ip] [password]
# Defaults: 10.3.38.19 / luckfox
```

### Manual

```bash
scp build/embedded_netmon root@10.3.38.19:/usr/bin/embedded_netmon
scp config/netmon.conf root@10.3.38.19:/etc/netmon.conf
ssh root@10.3.38.19 'chmod +x /usr/bin/embedded_netmon'
```

## Configuration

Edit `/etc/netmon.conf` on the target (or `config/netmon.conf` before deploying):

```ini
# Gateway to ping for connectivity check
ping_target = 10.0.0.1

# DNS server and test hostname
dns_target = 8.8.8.8
dns_test_hostname = google.com

# WiFi configuration
wifi_enabled = 1
wifi_ssid = YOUR_SSID
wifi_password = YOUR_PASSCODE

# Alert thresholds
alert_bw_warn_pct = 80
alert_bw_crit_pct = 95
alert_ping_warn_ms = 100
alert_ping_crit_ms = 500
alert_err_threshold = 10
```

## Project Structure

```
Embedded_Network_Monitor/
├── cmake/                  # Cross-compilation toolchain
├── config/                 # Default configuration file
├── deploy/                 # Deployment and init scripts
├── lib/
│   ├── lvgl/               # LVGL 8.3 (submodule)
│   └── lv_drivers/         # lv_drivers 8.1 (submodule)
├── src/
│   ├── main.c              # Entry point and main loop
│   ├── conf/               # Compile-time configuration
│   ├── core/               # Config, alerts, ring buffers, logging, utils
│   ├── hal/                # Framebuffer and touchscreen drivers
│   ├── net/                # Network data collectors, WiFi mgmt, WAN discovery
│   └── ui/                 # All UI screens and theming
├── CMakeLists.txt
├── lv_conf.h               # LVGL configuration
└── lv_drv_conf.h           # Driver configuration
```

## Timer Architecture

| Timer | Interval | Purpose |
|-------|----------|---------|
| Fast | 1s | Traffic stats from `/proc/net/dev`, rate computation, sparklines |
| Medium | 5s | ICMP ping, DNS probe, connection tracking, alert evaluation |
| Slow | 30s | Interface enumeration, ARP table, WiFi stats, WAN discovery |
| UI | 500ms | Refresh visible screen widgets |

## License

MIT

## Acknowledgments

- [LVGL](https://lvgl.io/) -- Light and Versatile Graphics Library
- [Luckfox](https://www.luckfox.com/) -- Luckfox Pico development platform
