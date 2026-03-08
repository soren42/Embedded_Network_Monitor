# Satellite Display Subsystem

A cockpit-style round display satellite for the Embedded Network Monitor. Receives
network monitoring data over USB serial from any host running the monitor, and
renders it on a circular gauge interface inspired by glass cockpit PFDs in
modern jet aircraft.

## Architecture

```
┌──────────────┐     USB Serial      ┌──────────────────┐
│  Host Device │    (115200 baud)     │  Satellite       │
│              │ ──────────────────── │  Display         │
│  Network     │   Binary protocol   │                  │
│  Monitor     │   ~100 bytes/sec    │  ESP32-S3 +      │
│  + Feeder    │                     │  Round LCD       │
└──────────────┘                     └──────────────────┘
```

The system has three components:

| Component | Language | Description |
|-----------|----------|-------------|
| **Protocol** | C header | Shared binary frame format (8-byte header + payload + CRC-16) |
| **Host Feeder** | C (Linux) | Polls REST API, encodes snapshots, streams over USB serial |
| **Firmware** | C (ESP-IDF) | Decodes frames, renders cockpit UI with LVGL on round display |

## Display Layout

```
         ╭────────────────╮
       ╱     NETWORK       ╲
     ╱   ● GW    ● DNS      ╲
    │    ● WAN   ● MEM        │
  R │                          │ T
  X │    GW    1.2 ms          │ X
  ▓ │    DNS  15.3 ms          │ ▓
  ▓ │    WAN  25.1 ms          │ ▓
  ▓ │   ─────────────         │ ▓
    │    eth0   ↓1.5M ↑800K    │
     ╲   wlan0  ↓2.1M ↑150K  ╱
       ╲    NO ALERTS        ╱
         ╰────────────────╯
```

- **Left arc (RX)** — Aggregate receive bandwidth utilization, fills bottom-to-top
- **Right arc (TX)** — Aggregate transmit bandwidth utilization, fills bottom-to-top
- **Color coding** — Green (0–60%) → Amber (60–85%) → Red (85–100%)
- **Status dots** — Gateway, DNS, WAN, Memory health at a glance
- **Center** — Latency readouts and per-interface rates
- **Bottom** — Alert count with severity color, uptime

## Supported Hardware

The firmware supports multiple Waveshare ESP32-S3 round display boards through
a HAL (Hardware Abstraction Layer) with board-specific implementations:

| Board | Display | Resolution | Touch | Audio | Status |
|-------|---------|-----------|-------|-------|--------|
| ESP32-S3-Touch-LCD-1.28 | GC9A01A (SPI) | 240×240 | CST816S | None | Reference |
| ESP32-S3-Touch-LCD-1.85C | ST77916 (QSPI) | 360×360 | CST816 | PCM5101 | Skeleton |
| ESP32-S3-Touch-LCD-1.46B | SPD2010 (QSPI) | 412×412 | SPD2010 | PCM5101 | Skeleton |

The 1.28" board has the most complete implementation. The 1.85C and 1.46B boards
have HAL skeletons that need their vendor-specific display initialization sequences
from Waveshare's example code.

To switch boards, change the source file in `firmware/main/CMakeLists.txt`:

```cmake
# Change board_ws_1_28.c to board_ws_1_85.c or board_ws_1_46.c
SRCS "main.c" "sat_receiver.c" "sat_data.c" "sat_ui.c" "sat_gauges.c"
     "boards/board_ws_1_85.c"
```

## Binary Protocol

The protocol is designed for efficiency over a low-speed serial link. All values
are little-endian.

### Frame Format

```
Offset  Size  Field
0       2     Magic (0x4E4D = "NM")
2       1     Version (1)
3       1     Message type
4       2     Payload length
6       2     Sequence number
8       N     Payload
8+N     2     CRC-16/CCITT
```

### Message Types

| Type | Value | Payload |
|------|-------|---------|
| SNAPSHOT | 0x01 | Full network state (18 + 20×interfaces bytes) |
| HEARTBEAT | 0x02 | Timestamp only (4 bytes) |
| ALERT | 0x03 | Single alert notification |
| ACK | 0x80 | Acknowledgment |

### Snapshot Payload

```
Offset  Size  Field
0       4     Unix timestamp
4       4     Uptime (seconds)
8       1     Memory used (0–100%)
9       1     Number of interfaces
10      1     Connectivity flags (GW|DNS|RESOLVE|WAN)
11      1     Active alert count
12      1     Max alert severity (0–3)
13      1     WAN packet loss (0–100%)
14      2     Gateway latency (ms × 10)
16      2     DNS latency (ms × 10)
18      2     WAN latency (ms × 10)
20      2     WAN jitter (ms × 10)
22      20×N  Interface data blocks
```

### Interface Data Block (20 bytes each)

```
Offset  Size  Field
0       8     Interface name (null-padded)
8       1     Flags (UP|RUNNING|WIRELESS)
9       1     Error rate (errors/sec, max 255)
10      4     RX rate (bytes/sec)
14      4     TX rate (bytes/sec)
18      2     Link speed (Mbps)
20      1     WiFi signal (dBm, or 0)
21      1     Reserved
```

Typical frame sizes: 48 bytes (1 iface), 108 bytes (4 ifaces), 188 bytes (8 ifaces).

## Building

### Host Feeder

```bash
cd satellite/host
mkdir build && cd build
cmake ..
make
```

The feeder links against cJSON from the parent project's `lib/` directory.

For cross-compilation (e.g., for the Luckfox Pico target):

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-rockchip.cmake ..
make
```

### Firmware

Requires [ESP-IDF v5.1+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/).

```bash
cd satellite/firmware
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

LVGL 8.3 is pulled automatically via the ESP-IDF component manager on first build.

## Usage

1. **Flash the firmware** to your ESP32-S3 display board
2. **Connect** the board to the host via USB
3. **Start the monitor** on the host (the main Embedded Network Monitor)
4. **Run the feeder**:

```bash
# Auto-detect serial port (usually /dev/ttyACM0)
./sat_feeder

# Or specify options
./sat_feeder -d /dev/ttyACM0 -u http://localhost:8080 -b 115200 -v
```

### Feeder Options

| Flag | Default | Description |
|------|---------|-------------|
| `-d` | `/dev/ttyACM0` | Serial device path |
| `-u` | `http://localhost:8080` | Network monitor API URL |
| `-b` | `115200` | Baud rate |
| `-i` | `1000` | Update interval (ms) |
| `-v` | off | Verbose output |

## Adding New Board Support

To add support for a new round display:

1. Create `firmware/main/boards/board_yourboard.c`
2. Implement the HAL interface (`sat_hal.h`):
   - `hal_display_init()` — Initialize display, return LVGL display
   - `hal_touch_init()` — Initialize touch, return LVGL input device
   - `hal_play_alert()` — Play alert tone (optional)
   - Set `SAT_DISP_HOR_RES` and `SAT_DISP_VER_RES`
3. Update `firmware/main/CMakeLists.txt` to reference your board file
4. The UI automatically scales to any square resolution

## File Structure

```
satellite/
├── README.md                          # This file
├── protocol/
│   └── sat_protocol.h                 # Shared binary protocol definition
├── host/
│   ├── CMakeLists.txt                 # Host feeder build
│   └── src/
│       ├── sat_feeder.c               # Main feeder daemon
│       ├── sat_serial.c/h             # Linux serial port I/O
│       └── sat_encoder.c/h            # JSON-to-binary encoder
└── firmware/
    ├── CMakeLists.txt                 # ESP-IDF project
    ├── sdkconfig.defaults             # ESP32-S3 defaults
    ├── partitions.csv                 # Flash partition table
    └── main/
        ├── CMakeLists.txt             # Component registration
        ├── idf_component.yml          # LVGL dependency
        ├── lv_conf.h                  # LVGL configuration
        ├── main.c                     # Entry point
        ├── sat_receiver.c/h           # USB CDC protocol decoder
        ├── sat_data.c/h               # Local data model
        ├── sat_ui.c/h                 # Cockpit UI manager
        ├── sat_gauges.c/h             # Arc gauge widgets
        ├── sat_hal.h                  # Hardware abstraction interface
        └── boards/
            ├── board_ws_1_28.c        # 240×240 GC9A01A (reference)
            ├── board_ws_1_85.c        # 360×360 ST77916
            └── board_ws_1_46.c        # 412×412 SPD2010
```
