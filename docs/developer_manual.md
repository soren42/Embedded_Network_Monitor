# LP Network Monitor -- Developer Guide & Code Standards

## Table of Contents

1. [Project Architecture Overview](#project-architecture-overview)
2. [Directory Structure](#directory-structure)
3. [Build System](#build-system)
4. [Module Descriptions](#module-descriptions)
5. [LVGL 8.3 Patterns](#lvgl-83-patterns)
6. [Adding a New Screen](#adding-a-new-screen)
7. [Adding a New Network Collector](#adding-a-new-network-collector)
8. [Code Standards](#code-standards)
9. [Data Flow](#data-flow)
10. [Memory Strategy](#memory-strategy)
11. [Timer Architecture](#timer-architecture)
12. [Testing Approach](#testing-approach)
13. [Deployment Workflow](#deployment-workflow)
14. [Key API Reference](#key-api-reference)

---

## Project Architecture Overview

LP Network Monitor is a single-threaded, event-driven C99 application built on the LVGL 8.3 GUI framework. It runs on a Luckfox Pico 86 Smart Panel (Rockchip RV1106, ARM Cortex-A7) with a 720x720 capacitive touchscreen.

The architecture follows a layered design:

```
+-------------------------------------------------------+
|                     UI Layer (LVGL)                    |
|  ui_manager  ui_dashboard  ui_traffic  ui_alerts ...   |
+-------------------------------------------------------+
|                    Core Layer                          |
|  alerts    config    data_store    ring_buffer    log  |
+-------------------------------------------------------+
|                   Network Layer                        |
|  net_collector  net_interfaces  net_ping  net_dns ...  |
+-------------------------------------------------------+
|                     HAL Layer                          |
|          hal_display (fb0)    hal_touch (evdev)        |
+-------------------------------------------------------+
|                   Linux Kernel                         |
|   /proc/net/*    /dev/fb0    /dev/input/event0         |
+-------------------------------------------------------+
```

**Key design principles:**

- **Single-threaded:** All work happens in the LVGL event loop via timers. No mutexes, no race conditions.
- **No dynamic allocation:** All buffers and data structures are statically sized at compile time.
- **Separation of concerns:** Network data collection is completely decoupled from the UI. Collectors write to a global state struct; the UI reads from it.
- **Lazy screen creation:** Screens are built on first access to reduce startup time and memory usage.

---

## Directory Structure

```
LP_Network_Monitoring/
|-- CMakeLists.txt              Root build configuration
|-- lv_conf.h                   LVGL configuration header
|-- lv_drv_conf.h               LVGL drivers configuration header
|
|-- cmake/
|   +-- arm-rockchip.cmake      Cross-compilation toolchain file
|
|-- config/
|   +-- netmon.conf             Default configuration file
|
|-- deploy/
|   |-- deploy.sh               Build-and-deploy script
|   +-- S99netmon               Buildroot init.d service script
|
|-- docs/                       Documentation (this directory)
|
|-- lib/
|   |-- lvgl/                   LVGL 8.3 library (git submodule)
|   +-- lv_drivers/             LVGL display/input drivers (git submodule)
|
|-- src/
|   |-- main.c                  Application entry point and main loop
|   |
|   |-- conf/
|   |   +-- app_conf.h          Compile-time configuration constants
|   |
|   |-- core/
|   |   |-- alerts.h / .c       Alert threshold engine with hysteresis
|   |   |-- config.h / .c       INI-style configuration file parser
|   |   |-- data_store.h / .c   History data persistence (save/load)
|   |   |-- log.h / .c          Logging subsystem
|   |   |-- ring_buffer.h / .c  Generic ring buffer for history data
|   |   +-- utils.h / .c        Formatting helpers (bytes, rates, IPs, MACs)
|   |
|   |-- hal/
|   |   |-- hal_display.h / .c  Framebuffer display driver integration
|   |   +-- hal_touch.h / .c    Evdev touchscreen driver integration
|   |
|   |-- net/
|   |   |-- net_collector.h / .c  Collection orchestrator and global state
|   |   |-- net_interfaces.h / .c Interface enumeration via ioctl
|   |   |-- net_ping.h / .c      ICMP echo probe
|   |   |-- net_dns.h / .c       DNS UDP probe
|   |   |-- net_arp.h / .c       ARP table reader (/proc/net/arp)
|   |   |-- net_wifi.h / .c      WiFi stats (/proc/net/wireless + ioctl)
|   |   +-- net_connections.h / .c TCP/UDP connection parser (/proc/net/tcp, udp)
|   |
|   +-- ui/
|       |-- ui_manager.h / .c    Screen lifecycle, navigation, status bar
|       |-- ui_theme.h / .c      Color palette, fonts, global styles
|       |-- ui_widgets.h / .c    Reusable widget factories (cards, LEDs, etc.)
|       |-- ui_dashboard.h / .c  Dashboard screen (overview)
|       |-- ui_interface.h / .c  Interface detail screen
|       |-- ui_traffic.h / .c    24-hour traffic chart screen
|       |-- ui_alerts.h / .c     Alert log screen
|       |-- ui_connections.h / .c Connection table screen
|       +-- ui_settings.h / .c   Settings/More screen
|
+-- build/                      Build output directory (generated)
```

---

## Build System

### CMake Configuration

The project uses CMake 3.10+ with the C99 standard:

```cmake
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
```

POSIX APIs are enabled via compile definitions:

```cmake
add_compile_definitions(_POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
```

### Cross-Compilation

The `cmake/arm-rockchip.cmake` toolchain file configures cross-compilation:

| Setting | Value |
|---------|-------|
| **Compiler** | `arm-linux-gnueabihf-gcc` |
| **Architecture flags** | `-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard` |
| **Linking** | `-static` (statically linked to avoid glibc/uClibc mismatch on target) |
| **Strip tool** | `arm-linux-gnueabihf-strip` (used in Release mode) |

### Build Commands

**Release build (for deployment):**

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-rockchip.cmake
make -j$(nproc)
```

**Debug build (for development):**

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-rockchip.cmake
make -j$(nproc)
```

### Build Outputs

| Mode | Binary Size | Optimization | Debug Symbols | Stripped |
|------|-------------|-------------|---------------|---------|
| Release | ~700 KB | `-Os` | No (`-DNDEBUG`) | Yes |
| Debug | ~2-3 MB | `-O0` | Yes (`-g3 -DDEBUG`) | No |

### Compiler Warnings

The project compiles with strict warning flags:

```
-Wall -Wextra -Wshadow -Wundef -Wmissing-prototypes
-Wpointer-arith -fno-strict-aliasing -Wno-unused-parameter
```

All warnings should be treated as errors in production code.

### Dependencies

| Library | Integration | Purpose |
|---------|-------------|---------|
| **LVGL 8.3** | `lib/lvgl/` (submodule, compiled via `add_subdirectory`) | GUI framework |
| **lv_drivers** | `lib/lv_drivers/` (submodule, compiled via `add_subdirectory`) | Linux framebuffer and evdev drivers for LVGL |
| **pthread** | System library (linked statically) | Required by LVGL internals |
| **libm** | System library (linked statically) | Math functions |
| **librt** | System library (linked statically) | POSIX real-time extensions |

---

## Module Descriptions

### HAL Layer (`src/hal/`)

The Hardware Abstraction Layer isolates hardware-specific initialization.

#### `hal_display` -- Framebuffer Display

- Opens `/dev/fb0` and registers an LVGL display driver.
- Provides a flush callback that writes pixel data to the framebuffer.
- Display resolution is fixed at 720x720 (`APP_DISP_HOR_RES` x `APP_DISP_VER_RES`).

#### `hal_touch` -- Touchscreen Input

- Opens `/dev/input/event0` (Goodix capacitive touch controller) and registers an LVGL input device driver.
- Provides a read callback that translates evdev touch events into LVGL input events.

### Network Layer (`src/net/`)

The Network layer collects all data from the Linux kernel. Each module is a focused collector.

#### `net_collector` -- Orchestrator

- Defines all data structures (`net_state_t`, `net_iface_state_t`, `net_rates_t`, etc.).
- Owns the global `net_state_t` instance.
- Registers LVGL timers at different frequencies to call individual collectors.
- Provides `net_get_state()` for read-only access to the global state.
- Manages short (60s) and long (24h) history ring buffers per interface.

#### `net_interfaces` -- Interface Enumeration

- Uses `ioctl(SIOCGIFCONF)` and related ioctls to enumerate non-loopback interfaces.
- Populates `net_iface_info_t` with name, flags (UP/RUNNING), IP, netmask, gateway, MAC, speed, and wireless status.

#### `net_ping` -- ICMP Probe

- Creates a raw ICMP socket and sends an echo request.
- Measures round-trip time and reports reachability.
- Requires root (raw socket).

#### `net_dns` -- DNS Probe

- Sends a minimal UDP DNS A-record query to the configured DNS server.
- Measures response time and verifies that the hostname resolves.

#### `net_arp` -- ARP Table

- Parses `/proc/net/arp` for complete Ethernet entries.
- Returns IP address, MAC address, and interface for each neighbor.

#### `net_wifi` -- WiFi Statistics

- Parses `/proc/net/wireless` for link quality, signal, and noise.
- Uses wireless ioctls (`SIOCGIWESSID`, `SIOCGIWRATE`) for SSID and bitrate.

#### `net_connections` -- Connection Table

- Parses `/proc/net/tcp` and `/proc/net/udp` for active connections.
- Returns local/remote addresses, ports, protocol, and TCP state.

### Core Layer (`src/core/`)

General-purpose modules used across the application.

#### `alerts` -- Alert Engine

- Evaluates network state against configured thresholds.
- Implements hysteresis: a condition must persist for `APP_ALERT_HYSTERESIS` (default 3) consecutive checks before triggering.
- Maintains a ring buffer of up to `APP_MAX_ALERTS` (64) alert entries.
- Supports 7 alert types and 3 severity levels (Info, Warning, Critical).

#### `config` -- Configuration Parser

- Loads INI-style key-value files.
- Provides typed getters: `config_get_str()`, `config_get_int()`, `config_get_double()`.
- Returns caller-supplied defaults for missing keys.
- Supports save-back for potential future settings modification.
- Maximum 32 entries, 64-character keys, 128-character values.

#### `data_store` -- History Persistence

- Saves history ring buffers to `/tmp/netmon_history.dat` on clean shutdown.
- Loads history from disk on startup so 24-hour charts survive restarts.

#### `ring_buffer` -- Generic Ring Buffer

- Fixed-capacity circular buffer for `float` values.
- Used for traffic history samples (both short and long term).
- Operations: `push`, `get`, `clear`, `full`, `empty`.
- Uses a caller-provided backing array (no allocation).

#### `log` -- Logging

- Four levels: DEBUG, INFO, WARN, ERROR.
- Printf-style formatting via `log_msg()`.
- Convenience macros: `LOG_DEBUG()`, `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()`.
- Default level at startup: `LOG_LEVEL_INFO`.

#### `utils` -- Formatting Helpers

- `format_bytes()` -- Human-readable byte counts (e.g., "1.5 GB").
- `format_rate()` -- Human-readable rates (e.g., "12.3 MB/s").
- `format_duration()` -- Uptime formatting (e.g., "2d 5h 12m").
- `format_ip()` -- uint32_t to dotted-decimal string.
- `format_mac()` -- 6-byte array to "AA:BB:CC:DD:EE:FF".
- `format_timestamp()` -- Epoch to human-readable timestamp.

### UI Layer (`src/ui/`)

The UI layer builds and updates all LVGL screens.

#### `ui_manager` -- Screen Lifecycle

- Creates the root layout: status bar (50 px), content area (610 px), navigation bar (60 px).
- Manages screen switching via `ui_manager_show()`.
- Lazily creates screens on first access.
- Provides a one-level back stack via `ui_manager_back()`.
- Maintains the alert badge count in the status bar.

#### `ui_theme` -- Visual Theming

- Defines the dark color palette used throughout the application.
- Creates and manages LVGL styles for screens, cards, titles, values, navigation, and status indicators.
- Provides font accessors (Montserrat 28/20/14).

Key colors:

| Constant | Hex | Usage |
|----------|-----|-------|
| `COLOR_BG` | `#1A1A2E` | Screen background |
| `COLOR_CARD` | `#16213E` | Card panel fill |
| `COLOR_CARD_BORDER` | `#0F3460` | Card border |
| `COLOR_PRIMARY` | `#00B4D8` | Primary accent |
| `COLOR_SUCCESS` | `#2ECC71` | Green / OK / TX |
| `COLOR_WARNING` | `#F39C12` | Yellow / Warning |
| `COLOR_DANGER` | `#E74C3C` | Red / Error / Critical |
| `COLOR_TEXT_PRIMARY` | `#E8E8E8` | Main text |
| `COLOR_TEXT_SECONDARY` | `#8892A0` | Secondary/muted text |
| `COLOR_NAV_BG` | `#0F3460` | Navigation bar background |
| `COLOR_CHART_RX` | `#3498DB` | Chart RX (blue) |
| `COLOR_CHART_TX` | `#2ECC71` | Chart TX (green) |

#### `ui_widgets` -- Reusable Components

Factory functions for common UI elements:

| Function | Creates |
|----------|---------|
| `ui_create_card()` | A styled rounded card panel with border. |
| `ui_create_stat_label()` | A two-line widget: small title label above a larger value label. |
| `ui_update_stat_label()` | Updates the value text of a stat-label widget. |
| `ui_create_status_led()` | A circular colored LED indicator. |
| `ui_set_status_led()` | Sets LED color by status code (0=red, 1=green, 2=yellow). |
| `ui_create_section_header()` | A bold section header label. |
| `ui_create_back_btn()` | A back-arrow button that calls `ui_manager_back()`. |

#### Screen Modules

| Module | Screen | Description |
|--------|--------|-------------|
| `ui_dashboard` | `SCREEN_DASHBOARD` | Interface cards with sparklines, connectivity panel. |
| `ui_interface` | `SCREEN_IFACE_DETAIL` | Full interface statistics, 60s chart, WiFi info. |
| `ui_traffic` | `SCREEN_TRAFFIC` | 24-hour traffic chart with interface selector. |
| `ui_alerts` | `SCREEN_ALERTS` | Scrollable alert log with severity indicators. |
| `ui_connections` | `SCREEN_CONNECTIONS` | TCP/UDP connection table. |
| `ui_settings` | `SCREEN_SETTINGS` / `SCREEN_MORE` | Configuration display, navigation to sub-screens, system info. |

---

## LVGL 8.3 Patterns

### Timer-Driven Updates

All periodic work is driven by LVGL timers rather than threads. Timers are created with `lv_timer_create()` and fire at configured intervals within the main event loop.

```c
/* Example: register a timer that fires every 1000 ms */
lv_timer_create(fast_timer_cb, APP_FAST_TIMER_MS, NULL);
```

The main loop calls `lv_timer_handler()` to process pending timer callbacks and `lv_tick_inc()` to advance the LVGL tick counter.

### Style System

Styles are defined once in `ui_theme_init()` and applied to objects via `lv_obj_add_style()`. Each style is a static `lv_style_t` initialized with `lv_style_init()` and configured with property setters.

```c
static lv_style_t style_card;
lv_style_init(&style_card);
lv_style_set_bg_color(&style_card, COLOR_CARD);
lv_style_set_border_color(&style_card, COLOR_CARD_BORDER);
lv_style_set_border_width(&style_card, 2);
lv_style_set_radius(&style_card, 12);
lv_style_set_pad_all(&style_card, 12);
```

### Flex Layout

LVGL's flex layout is used extensively for responsive arrangement of elements:

```c
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
```

Common patterns:
- `LV_FLEX_FLOW_COLUMN` -- Vertical stacking (most screens).
- `LV_FLEX_FLOW_ROW` -- Horizontal arrangement (stat label groups, navigation tabs).
- `LV_FLEX_FLOW_ROW_WRAP` -- Wrapping horizontal layout (interface cards on dashboard).

### Charts

LVGL charts (`lv_chart`) are used for traffic visualization:

```c
lv_obj_t *chart = lv_chart_create(parent);
lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
lv_chart_set_point_count(chart, APP_HISTORY_SHORT_LEN);  /* 60 points */

lv_chart_series_t *rx_ser = lv_chart_add_series(chart, COLOR_CHART_RX, LV_CHART_AXIS_PRIMARY_Y);
lv_chart_series_t *tx_ser = lv_chart_add_series(chart, COLOR_CHART_TX, LV_CHART_AXIS_PRIMARY_Y);
```

Data is pushed to chart series from ring buffers during timer callbacks.

### Scrollable Containers

Screens with variable-length content (alerts, connections) use LVGL's built-in scrollbar:

```c
lv_obj_set_scroll_dir(container, LV_DIR_VER);
lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_AUTO);
```

---

## Adding a New Screen

Follow these steps to add a new screen to the application:

### Step 1: Define the Screen ID

Add a new entry to the `screen_id_t` enum in `src/ui/ui_manager.h`, before `SCREEN_COUNT`:

```c
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_TRAFFIC,
    SCREEN_ALERTS,
    SCREEN_MORE,
    SCREEN_IFACE_DETAIL,
    SCREEN_CONNECTIONS,
    SCREEN_SETTINGS,
    SCREEN_MY_NEW_SCREEN,    /* <-- Add here */
    SCREEN_COUNT
} screen_id_t;
```

### Step 2: Create the Screen Module

Create `src/ui/ui_my_screen.h`:

```c
#ifndef UI_MY_SCREEN_H
#define UI_MY_SCREEN_H

#include "lvgl.h"

void ui_my_screen_create(lv_obj_t *parent);
void ui_my_screen_update(void);

#endif /* UI_MY_SCREEN_H */
```

Create `src/ui/ui_my_screen.c`:

```c
#include "ui_my_screen.h"
#include "ui_widgets.h"
#include "ui_theme.h"
#include "net_collector.h"

void ui_my_screen_create(lv_obj_t *parent)
{
    /* Add a back button if this is a sub-screen */
    ui_create_back_btn(parent);

    /* Build your UI widgets here, parented to `parent` */
    ui_create_section_header(parent, "My New Screen");
}

void ui_my_screen_update(void)
{
    /* Read from net_get_state() and update your widgets */
    const net_state_t *state = net_get_state();
    /* ... */
}
```

### Step 3: Register in the UI Manager

In `src/ui/ui_manager.c`, add the include and wire up the screen creation and update in the appropriate switch-case blocks (for lazy creation and periodic updates).

### Step 4: Add Navigation

To make the screen accessible, either:
- Add a button in an existing screen (e.g., the More screen) that calls `ui_manager_show(SCREEN_MY_NEW_SCREEN)`.
- Add a new tab in the navigation bar (if it is a top-level screen).

### Step 5: Build and Test

The CMake build system automatically picks up new `.c` files in `src/` via `GLOB_RECURSE`, so no CMakeLists.txt changes are needed.

---

## Adding a New Network Collector

### Step 1: Define the Data Structure

Add the relevant fields to the `net_state_t` struct in `src/net/net_collector.h`:

```c
typedef struct {
    /* existing fields ... */

    /* New: my custom data */
    int                my_custom_count;
    my_custom_entry_t  my_custom_data[MY_CUSTOM_MAX];
} net_state_t;
```

### Step 2: Create the Collector Module

Create `src/net/net_my_collector.h`:

```c
#ifndef NET_MY_COLLECTOR_H
#define NET_MY_COLLECTOR_H

#include "net_collector.h"

int net_collect_my_data(my_custom_entry_t *entries, int max_count);

#endif /* NET_MY_COLLECTOR_H */
```

Create `src/net/net_my_collector.c`:

```c
#include "net_my_collector.h"
#include <stdio.h>

int net_collect_my_data(my_custom_entry_t *entries, int max_count)
{
    /* Read from /proc or /sys as appropriate */
    /* Populate entries[] */
    /* Return count of entries, or -1 on error */
}
```

### Step 3: Integrate with the Collector Orchestrator

In `src/net/net_collector.c`, call your collector function from the appropriate timer callback:

- **Fast timer (1s):** For data that changes rapidly (traffic stats, rates).
- **Medium timer (5s):** For data that changes occasionally (ping, DNS, connections, alerts).
- **Slow timer (30s):** For data that rarely changes (interface enumeration, ARP table).

```c
static void medium_timer_cb(lv_timer_t *timer)
{
    /* existing collectors ... */

    /* New collector */
    g_state.my_custom_count = net_collect_my_data(
        g_state.my_custom_data, MY_CUSTOM_MAX);
}
```

### Step 4: Add Compile-Time Limits

Add the maximum array size to `src/conf/app_conf.h`:

```c
#define MY_CUSTOM_MAX  32
```

---

## Code Standards

### Language

- **Standard:** C99 (`-std=c99`), no GNU extensions (`CMAKE_C_EXTENSIONS OFF`).
- **POSIX:** `_POSIX_C_SOURCE=200809L` and `_DEFAULT_SOURCE` are defined for POSIX API access.

### Naming Conventions

| Element | Convention | Examples |
|---------|-----------|----------|
| **Functions** | `module_verb_noun` (snake_case) | `net_ping()`, `ui_create_card()`, `config_get_int()` |
| **Types (structs)** | `module_noun_t` (snake_case with `_t` suffix) | `net_state_t`, `alert_entry_t`, `config_t` |
| **Types (enums)** | `module_noun_t` for type, `MODULE_VALUE` for values | `screen_id_t`, `SCREEN_DASHBOARD` |
| **Macros / Constants** | `MODULE_NOUN_NAME` (UPPER_SNAKE_CASE) | `APP_MAX_INTERFACES`, `LOG_LEVEL_INFO` |
| **Local variables** | `snake_case` | `rx_bytes`, `iface_idx` |
| **Global statics** | `g_noun` prefix | `g_state`, `g_config`, `g_running` |
| **File-static** | `s_noun` prefix or no prefix | `s_history`, or context-obvious names |

### Module Prefixes

Each module uses a consistent prefix for all public symbols:

| Module | Prefix |
|--------|--------|
| `net_collector` | `net_` |
| `net_interfaces` | `net_enumerate_` |
| `net_ping` | `net_ping` |
| `net_dns` | `net_dns_` |
| `net_arp` | `net_read_arp` |
| `net_wifi` | `net_wifi_` |
| `net_connections` | `net_collect_connections` |
| `alerts` | `alerts_` |
| `config` | `config_` |
| `data_store` | `data_store_` |
| `ring_buffer` | `ring_buffer_` |
| `log` | `log_` / `LOG_` |
| `utils` | `format_` |
| `ui_manager` | `ui_manager_` |
| `ui_theme` | `ui_theme_` / `ui_get_style_` / `ui_font_` |
| `ui_widgets` | `ui_create_` / `ui_set_` / `ui_update_` |
| `ui_dashboard` | `ui_dashboard_` |
| Screen modules | `ui_<screen>_` |

### Header Guard Convention

```c
#ifndef MODULE_NAME_H
#define MODULE_NAME_H
/* ... */
#endif /* MODULE_NAME_H */
```

### Include Order

1. Corresponding header for the `.c` file.
2. Project headers (alphabetical).
3. LVGL headers.
4. POSIX / system headers.
5. Standard C library headers.

### Error Handling

- Functions that can fail return `-1` on error and `0` or a positive count on success.
- Critical failures (display init, touch init) cause the application to exit with a non-zero code.
- Non-critical failures (missing config file, individual collector errors) log a warning and continue with defaults.

### Documentation

- Every public function has a Doxygen-style comment in the header file.
- Implementation files may have inline comments for non-obvious logic.
- No redundant comments for self-explanatory code.

---

## Data Flow

The application's data flow is strictly one-directional:

```
Linux Kernel (/proc, ioctl, sockets)
        |
        v
Network Collectors (net_*.c)
        |
        |  write
        v
Global State (net_state_t)  <-----  History Ring Buffers
        |                              |
        |  read-only                   |  read-only
        v                              v
Alert Engine (alerts.c)          UI Screens (ui_*.c)
        |
        |  write
        v
Alert Log (alert_entry_t[])
        |
        |  read-only
        v
UI Alert Screen + Badge
```

### Collection Cycle

1. **Fast timer (every 1 second):**
   - Read `/proc/net/dev` for raw byte/packet counters.
   - Compute rates (delta / elapsed time).
   - Push rate samples into the short-term (60s) ring buffer.
   - Push averaged samples into the long-term (24h) ring buffer (every 60th fast tick).
   - Track peak rates and daily totals.

2. **Medium timer (every 5 seconds):**
   - Send ICMP ping to configured gateway, measure latency.
   - Send DNS probe to configured server, measure latency.
   - Collect TCP/UDP connections from `/proc/net/tcp` and `/proc/net/udp`.
   - Run alert threshold checks against current state.

3. **Slow timer (every 30 seconds):**
   - Re-enumerate network interfaces (detect new/removed interfaces).
   - Read ARP table from `/proc/net/arp`.
   - Read WiFi statistics for wireless interfaces.
   - Read system info (uptime, memory) from `/proc`.

### UI Update Cycle

The UI refresh timer fires every 500 ms (`APP_UI_REFRESH_MS`). Each screen module has an `_update()` function that:

1. Calls `net_get_state()` to get a pointer to the current global state.
2. Reads the relevant fields.
3. Formats values using `utils.h` functions.
4. Updates LVGL labels, LEDs, and charts.

Only the currently visible screen's `_update()` function is called, minimizing unnecessary work.

---

## Memory Strategy

### No Dynamic Allocation

The application uses zero calls to `malloc()`, `calloc()`, `realloc()`, or `free()`. All data structures are statically allocated at compile time. This eliminates:

- Memory leaks.
- Heap fragmentation (critical on embedded systems with limited RAM).
- Non-deterministic allocation times.

### Static Limits

All array sizes are defined as compile-time constants in `src/conf/app_conf.h`:

| Constant | Value | Purpose |
|----------|-------|---------|
| `APP_MAX_INTERFACES` | 8 | Maximum number of network interfaces tracked. |
| `APP_MAX_CONNECTIONS` | 256 | Maximum TCP/UDP connections displayed. |
| `APP_MAX_ALERTS` | 64 | Alert log ring buffer capacity. |
| `APP_HISTORY_SHORT_LEN` | 60 | 60-second history at 1s resolution. |
| `APP_HISTORY_LONG_LEN` | 1440 | 24-hour history at 1-minute resolution. |
| `APP_MAX_ARP_ENTRIES` | 64 | Maximum ARP table entries. |
| `APP_IFACE_NAME_MAX` | 16 | Maximum interface name length. |
| `CONFIG_MAX_ENTRIES` | 32 | Maximum configuration key-value pairs. |
| `CONFIG_KEY_MAX` | 64 | Maximum configuration key length. |
| `CONFIG_VAL_MAX` | 128 | Maximum configuration value length. |

### Ring Buffers

Ring buffers (`ring_buffer_t`) are initialized with a pointer to a pre-allocated static `float` array:

```c
static float s_short_rx[APP_HISTORY_SHORT_LEN];
static ring_buffer_t s_short_rx_rb;

ring_buffer_init(&s_short_rx_rb, s_short_rx, APP_HISTORY_SHORT_LEN);
```

When the buffer is full, new values overwrite the oldest, maintaining a sliding window.

### LVGL Memory

LVGL has its own memory management configured in `lv_conf.h`. It uses a statically allocated memory pool. The pool size should be tuned to accommodate all screens, widgets, and chart data. Screens are lazily created to spread memory usage over time rather than allocating everything at startup.

### Runtime Footprint

- **Binary size:** ~700 KB (statically linked, stripped).
- **RSS:** ~3 MB (includes LVGL framebuffer, memory pool, and all static data).

---

## Timer Architecture

All periodic work is organized into four timer tiers, each implemented as an LVGL timer:

| Timer | Interval | Purpose | Data Sources |
|-------|----------|---------|--------------|
| **Fast** | 1,000 ms (`APP_FAST_TIMER_MS`) | Traffic statistics and rate computation | `/proc/net/dev` |
| **Medium** | 5,000 ms (`APP_MEDIUM_TIMER_MS`) | Connectivity probes and alert evaluation | ICMP ping, DNS probe, `/proc/net/tcp`, `/proc/net/udp` |
| **Slow** | 30,000 ms (`APP_SLOW_TIMER_MS`) | Interface enumeration, ARP, WiFi, system info | `ioctl`, `/proc/net/arp`, `/proc/net/wireless`, `/proc/meminfo` |
| **UI Refresh** | 500 ms (`APP_UI_REFRESH_MS`) | Update visible screen widgets | Reads from `net_state_t` |

### Timer Rationale

- **Fast (1s):** Traffic rates need frequent sampling for responsive sparklines and accurate rate calculations.
- **Medium (5s):** Ping and DNS probes are network operations with latency; running them more frequently would add unnecessary load and network traffic.
- **Slow (30s):** Interface enumeration and ARP table changes are infrequent. The ioctl calls are relatively expensive.
- **UI (500ms):** 2 FPS update rate provides smooth-looking counters and charts without excessive CPU usage from LVGL rendering.

### LVGL Tick

The main loop advances the LVGL tick counter based on wall-clock time:

```c
uint64_t now_ms = get_time_ms();
uint32_t elapsed = (uint32_t)(now_ms - prev_ms);
prev_ms = now_ms;
lv_tick_inc(elapsed);
lv_timer_handler();
usleep(APP_TICK_PERIOD_MS * 1000);  /* 5 ms sleep */
```

The 5 ms tick period (`APP_TICK_PERIOD_MS`) provides smooth touch input handling and animation while keeping CPU usage low.

---

## Testing Approach

### On-Target Testing

Since the application is tightly coupled to hardware (framebuffer, touchscreen, `/proc` filesystem), primary testing is done on the target device:

1. **Deploy** the debug build to the target.
2. **Run in foreground** to see log output: `/usr/bin/lp_netmon /etc/netmon.conf`
3. **Verify each screen** by navigating through all tabs and sub-screens.
4. **Generate test traffic** (e.g., `iperf3`, `ping -f`, `wget`) to exercise rate calculations and charts.
5. **Simulate failures** (disconnect cable, bring interface down) to test alerts.

### Unit Testing Strategy

Individual modules can be tested on the host by mocking dependencies:

- **`config.c`** -- Test with sample INI files.
- **`ring_buffer.c`** -- Pure data structure, fully testable on host.
- **`utils.c`** -- Pure formatting functions, fully testable on host.
- **`alerts.c`** -- Can be tested with mock `net_state_t` data.

Host-side tests can be compiled with the native compiler (no cross-compilation needed) since these modules have no hardware dependencies.

### Integration Verification

After deploying a new version, verify:

- [ ] Application starts without errors.
- [ ] All interfaces are detected and displayed.
- [ ] Traffic rates update every second.
- [ ] Sparklines show data movement.
- [ ] Tapping interface cards opens detail screen.
- [ ] 24-hour chart populates over time.
- [ ] Ping and DNS indicators show correct status.
- [ ] Alerts trigger when thresholds are exceeded.
- [ ] Alerts clear with hysteresis.
- [ ] Connections table shows active connections.
- [ ] Back button and tab navigation work correctly.
- [ ] Application handles interface up/down gracefully.
- [ ] Clean shutdown saves history data.
- [ ] History data loads on restart (24h chart retains data).

---

## Deployment Workflow

### Standard Development Cycle

```
1. Edit source code on development host
         |
         v
2. Build:  cd build && make -j$(nproc)
         |
         v
3. Deploy: ../deploy/deploy.sh 10.3.38.19 luckfox
         |
         v
4. Test:   ssh root@10.3.38.19 '/etc/init.d/S99netmon restart'
         |
         v
5. Verify: ssh root@10.3.38.19 '/etc/init.d/S99netmon status'
         |
         v
6. Debug (if needed): ssh and run foreground
```

### Quick Iteration (Binary Only)

For rapid development when only code has changed (no config or init script changes):

```bash
# On the development host:
cd build && make -j$(nproc)
scp lp_netmon root@10.3.38.19:/usr/bin/lp_netmon

# On the target (or via SSH):
/etc/init.d/S99netmon restart
```

### Full Rebuild

If CMake configuration or toolchain has changed:

```bash
rm -rf build/*
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-rockchip.cmake
make -j$(nproc)
```

---

## Key API Reference

### Network State Access

```c
/* Get read-only pointer to the global network state.
 * Always valid after net_collector_init() has been called.
 * Updated by collector timers. Safe to read from UI timer callbacks. */
const net_state_t *net_get_state(void);
```

**Usage:**

```c
const net_state_t *state = net_get_state();
for (int i = 0; i < state->num_ifaces; i++) {
    const net_iface_state_t *iface = &state->ifaces[i];
    printf("%s: RX %.1f KB/s, TX %.1f KB/s\n",
           iface->info.name,
           iface->rates.rx_bytes_per_sec / 1024.0,
           iface->rates.tx_bytes_per_sec / 1024.0);
}
```

### History Access

```c
/* Get the 60-second (1s resolution) history for an interface. */
void net_get_short_history(int iface_idx,
                           const net_history_sample_t **out_data,
                           int *out_count, int *out_head, int *out_capacity);

/* Get the 24-hour (1-min resolution) history for an interface. */
void net_get_long_history(int iface_idx,
                          const net_history_sample_t **out_data,
                          int *out_count, int *out_head, int *out_capacity);
```

### Screen Navigation

```c
/* Switch to a screen by ID. Lazily creates the screen on first access. */
void ui_manager_show(screen_id_t screen);

/* Navigate to the interface detail screen for a specific interface. */
void ui_manager_show_iface_detail(int iface_idx);

/* Return to the previous screen (one-level back stack). */
void ui_manager_back(void);

/* Get the currently displayed screen ID. */
screen_id_t ui_manager_current(void);

/* Update the alert badge in the status bar. Pass 0 to hide. */
void ui_manager_set_alert_count(int count);

/* Get the content container for parenting screen widgets. */
lv_obj_t *ui_manager_get_content(void);
```

### Alert System

```c
/* Initialize alerts with configuration (thresholds). */
void alerts_init(const config_t *cfg);

/* Evaluate all thresholds against current network state. */
void alerts_check(const void *state);

/* Get alert log entries. Returns count of currently active alerts. */
int alerts_get_log(const alert_entry_t **out_entries, int *out_count);

/* Get count of active alerts. */
int alerts_active_count(void);

/* Clear all active alerts. */
void alerts_clear_all(void);
```

### Configuration

```c
/* Load config from file. Returns 0 on success, -1 on error. */
int config_load(config_t *cfg, const char *path);

/* Get a string value, or default_val if key is not found. */
const char *config_get_str(const config_t *cfg, const char *key,
                           const char *default_val);

/* Get an integer value, or default_val if key is not found. */
int config_get_int(const config_t *cfg, const char *key, int default_val);

/* Get a double value, or default_val if key is not found. */
double config_get_double(const config_t *cfg, const char *key,
                         double default_val);
```

### Widget Factories

```c
/* Create a styled card panel. */
lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/* Create a title/value stat label pair. */
lv_obj_t *ui_create_stat_label(lv_obj_t *parent, const char *title);

/* Update the value text in a stat label. */
void ui_update_stat_label(lv_obj_t *stat_obj, const char *value);

/* Create a circular status LED. */
lv_obj_t *ui_create_status_led(lv_obj_t *parent, lv_coord_t size);

/* Set LED color: 0=red, 1=green, 2=yellow. */
void ui_set_status_led(lv_obj_t *led, int status);

/* Create a section header. */
lv_obj_t *ui_create_section_header(lv_obj_t *parent, const char *text);

/* Create a back button (top-left, calls ui_manager_back). */
lv_obj_t *ui_create_back_btn(lv_obj_t *parent);
```

### Logging

```c
/* Initialize logging with minimum level. */
void log_init(int level);

/* Log a message at the given level. */
void log_msg(int level, const char *fmt, ...);

/* Convenience macros: */
LOG_DEBUG("value = %d", x);
LOG_INFO("Started on %s", iface);
LOG_WARN("Missing config key: %s", key);
LOG_ERROR("Failed to open %s: %s", path, strerror(errno));
```

### Formatting Utilities

```c
void format_bytes(uint64_t bytes, char *buf, int buf_len);
/* Example output: "1.5 GB", "340 KB", "12 B" */

void format_rate(double bytes_per_sec, char *buf, int buf_len);
/* Example output: "12.3 MB/s", "1.1 KB/s" */

void format_duration(uint32_t seconds, char *buf, int buf_len);
/* Example output: "2d 5h 12m", "45m 30s" */

void format_ip(uint32_t ip, char *buf, int buf_len);
/* Example output: "192.168.1.1" */

void format_mac(const uint8_t mac[6], char *buf, int buf_len);
/* Example output: "AA:BB:CC:DD:EE:FF" */

void format_timestamp(uint32_t epoch, char *buf, int buf_len);
/* Example output: "2026-02-18 14:30:05" */
```
