# LP Network Monitor

A real-time network monitoring application for the [Luckfox Pico 86 Smart Panel](https://www.luckfox.com/) (Model 1408). Built in C99 with [LVGL 8.3](https://lvgl.io/) for a responsive, touch-driven dark-themed UI on the panel's 720x720 display.

![Platform](https://img.shields.io/badge/platform-Luckfox%20Pico%2086-blue)
![Language](https://img.shields.io/badge/language-C99-green)
![UI](https://img.shields.io/badge/UI-LVGL%208.3-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Features

- **Dashboard** -- At-a-glance status for all network interfaces with live sparkline charts, RX/TX rates, and connectivity indicators
- **Interface Detail** -- Per-interface drill-down with traffic stats, peak rates, daily totals, error/drop counts, and WiFi info (SSID, signal, bitrate)
- **Traffic Charts** -- 24-hour bandwidth history at 1-minute resolution with interface selector
- **Alert System** -- Threshold-based alerts with hysteresis for interface down, high bandwidth, packet errors/drops, gateway unreachable, DNS failure, and high latency
- **Connection Tracking** -- Live TCP/UDP connection table parsed from `/proc/net/tcp` and `/proc/net/udp`
- **Settings** -- Network configuration display, alert thresholds, and system info

## Screenshots

*The UI runs on the 720x720 capacitive touchscreen with a dark theme optimized for at-a-glance monitoring.*

## Hardware

| Component | Specification |
|-----------|--------------|
| **Board** | Luckfox Pico 86 Smart Panel, Model 1408 |
| **SoC** | Rockchip RV1106 (ARM Cortex-A7, single-core) |
| **Display** | 720x720 IPS, 32-bit ARGB8888 framebuffer |
| **Touch** | Goodix capacitive touchscreen (evdev) |
| **OS** | Buildroot Linux 5.10.160 |

## Architecture

```
┌─────────────────────────────────────────┐
│  UI Layer (LVGL 8.3)                    │
│  Dashboard, Detail, Traffic, Alerts,    │
│  Connections, Settings                  │
├─────────────────────────────────────────┤
│  Core Layer                             │
│  Config, Alerts Engine, Ring Buffers,   │
│  Data Persistence, Logging, Formatters  │
├─────────────────────────────────────────┤
│  Network Layer                          │
│  /proc parsers, ICMP Ping, DNS Probe,  │
│  ARP, WiFi, Connection Tracking        │
├─────────────────────────────────────────┤
│  HAL Layer                              │
│  Framebuffer (/dev/fb0)                 │
│  Evdev Touch (/dev/input/event0)        │
└─────────────────────────────────────────┘
```

**Key design decisions:**
- **Single-threaded** cooperative event loop using LVGL timers (1-core CPU)
- **All static allocation** -- zero calls to `malloc`, no heap fragmentation
- **~700 KB** stripped binary, **~3 MB** RSS at runtime
- Data collected from `/proc` and `ioctl` at 1s/5s/30s intervals

## Building

### Prerequisites

A Linux host (tested on Raspberry Pi / aarch64) with:

```bash
sudo apt-get install -y cmake gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf sshpass
```

### Clone

```bash
git clone --recursive https://github.com/soren42/LP_Network_Monitoring.git
cd LP_Network_Monitoring
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

The resulting `lp_netmon` binary is a statically-linked ARM ELF.

## Deployment

### Automated

```bash
./deploy/deploy.sh [target_ip] [password]
# Defaults: 10.3.38.19 / luckfox
```

This builds the project, deploys the binary to `/usr/bin/lp_netmon`, copies the config to `/etc/netmon.conf`, and installs the init script.

### Manual

```bash
scp build/lp_netmon root@10.3.38.19:/usr/bin/lp_netmon
scp config/netmon.conf root@10.3.38.19:/etc/netmon.conf
scp deploy/S99netmon root@10.3.38.19:/etc/init.d/S99netmon
ssh root@10.3.38.19 'chmod +x /etc/init.d/S99netmon && /etc/init.d/S99netmon restart'
```

### Disable Stock Demo App

```bash
./deploy/disable_demo.sh [target_ip] [password]
```

## Configuration

Edit `/etc/netmon.conf` on the target (or `config/netmon.conf` before deploying):

```ini
# Gateway to ping for connectivity check
ping_target=10.0.0.1

# DNS server and test hostname
dns_target=8.8.8.8
dns_test_hostname=example.com

# Alert thresholds
alert_bw_warning_pct=80
alert_ping_warning_ms=100
alert_ping_critical_ms=500
alert_drop_threshold=100
alert_error_threshold=50
```

## Project Structure

```
LP_Network_Monitoring/
├── cmake/                  # Cross-compilation toolchain
├── config/                 # Default configuration file
├── deploy/                 # Deployment and init scripts
├── docs/                   # Documentation (user, admin, developer)
├── lib/
│   ├── lvgl/               # LVGL 8.3 (submodule)
│   └── lv_drivers/         # lv_drivers 8.1 (submodule)
├── src/
│   ├── main.c              # Entry point and main loop
│   ├── conf/               # Compile-time configuration
│   ├── core/               # Config, alerts, ring buffers, logging, utils
│   ├── hal/                # Framebuffer and touchscreen drivers
│   ├── net/                # Network data collectors
│   └── ui/                 # All UI screens and theming
├── CMakeLists.txt
├── lv_conf.h               # LVGL configuration
└── lv_drv_conf.h           # Driver configuration
```

## Documentation

- [User Manual](docs/user_manual.md) -- Screen-by-screen guide, navigation, configuration reference
- [Admin Manual](docs/admin_manual.md) -- Installation, service management, troubleshooting, backup/restore
- [Developer Manual](docs/developer_manual.md) -- Architecture, code standards, API reference, how to extend

## Timer Architecture

| Timer | Interval | Purpose |
|-------|----------|---------|
| Fast | 1s | Traffic stats from `/proc/net/dev`, rate computation, sparklines |
| Medium | 5s | ICMP ping, DNS probe, connection tracking, alert evaluation |
| Slow | 30s | Interface enumeration, ARP table, WiFi stats, system info |
| UI | 500ms | Refresh visible screen widgets |

## License

MIT

## Acknowledgments

- [LVGL](https://lvgl.io/) -- Light and Versatile Graphics Library
- [Luckfox](https://www.luckfox.com/) -- Luckfox Pico development platform
