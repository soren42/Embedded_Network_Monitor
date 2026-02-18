# LP Network Monitor -- Administration & Troubleshooting Manual

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Installation](#installation)
3. [Service Management](#service-management)
4. [Disabling the Demo Application](#disabling-the-demo-application)
5. [Configuration Reference](#configuration-reference)
6. [Boot Integration](#boot-integration)
7. [Updating the Application](#updating-the-application)
8. [Troubleshooting](#troubleshooting)
9. [Monitoring Resource Usage](#monitoring-resource-usage)
10. [Log Messages](#log-messages)
11. [Network Requirements](#network-requirements)
12. [Backup and Restore](#backup-and-restore)

---

## System Requirements

### Hardware

| Component | Requirement |
|-----------|-------------|
| **Board** | Luckfox Pico 86 Smart Panel, Model 1408 |
| **SoC** | Rockchip RV1106 (ARM Cortex-A7) |
| **Display** | Integrated 720x720 capacitive touchscreen (Goodix controller) |
| **RAM** | Minimum 32 MB free (application uses approximately 3 MB RSS) |
| **Storage** | Approximately 1 MB free for the binary and configuration files |

### Software

| Component | Requirement |
|-----------|-------------|
| **OS** | Buildroot Linux (as shipped with the Luckfox Pico 86) |
| **Kernel** | Linux with framebuffer support (`/dev/fb0`) and evdev input (`/dev/input/event0`) |
| **Init system** | Buildroot init.d (BusyBox init with `/etc/init.d/` scripts) |
| **Utilities** | `start-stop-daemon` (provided by BusyBox) |

### Network

The application requires at least one active network interface. It monitors all non-loopback interfaces automatically. ICMP ping probes require raw socket access, which requires the application to run as `root`.

---

## Installation

### Automated Deployment (Recommended)

The `deploy.sh` script builds the application on the development host and deploys it to the target device over SSH.

**Prerequisites on the development host:**
- `arm-linux-gnueabihf-gcc` cross-compiler toolchain
- CMake 3.10 or later
- `sshpass` (for automated SSH password authentication)
- `make`

**Run the deployment:**

```bash
cd /path/to/LP_Network_Monitoring/deploy
./deploy.sh [target_ip] [password]
```

Default values if arguments are omitted:
- Target IP: `10.3.38.19`
- Password: `luckfox`

The script performs four steps:

1. **Build** -- Runs CMake with the ARM Rockchip cross-compilation toolchain and builds the `lp_netmon` binary in Release mode (optimized, stripped).
2. **Deploy binary** -- Copies `lp_netmon` to `/usr/bin/lp_netmon` on the target.
3. **Deploy config** -- Copies `netmon.conf` to `/etc/netmon.conf` on the target.
4. **Deploy init script** -- Copies `S99netmon` to `/etc/init.d/S99netmon` on the target and makes it executable.

After deployment, start the application:

```bash
sshpass -p 'luckfox' ssh root@10.3.38.19 '/etc/init.d/S99netmon restart'
```

### Manual Installation

If you prefer to install manually or the automated script is not suitable:

1. **Build the binary** on your development host:

   ```bash
   mkdir -p build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-rockchip.cmake
   make -j$(nproc)
   ```

2. **Copy files to the target** (via SCP, SD card, or other transfer method):

   ```
   build/lp_netmon       -->  /usr/bin/lp_netmon
   config/netmon.conf    -->  /etc/netmon.conf
   deploy/S99netmon      -->  /etc/init.d/S99netmon
   ```

3. **Set permissions** on the target:

   ```bash
   chmod +x /usr/bin/lp_netmon
   chmod +x /etc/init.d/S99netmon
   ```

4. **Start the service:**

   ```bash
   /etc/init.d/S99netmon start
   ```

---

## Service Management

The application is managed by the `/etc/init.d/S99netmon` init script, which supports the standard Buildroot service commands.

### Commands

| Command | Description |
|---------|-------------|
| `/etc/init.d/S99netmon start` | Start the network monitor. Automatically kills the demo app (`86UI_demo`) if it is running. Runs `lp_netmon` as a background daemon. |
| `/etc/init.d/S99netmon stop` | Stop the network monitor by sending `SIGTERM`. The application performs a clean shutdown, saving history data before exiting. |
| `/etc/init.d/S99netmon restart` | Stop and then start the application. A 1-second delay is included between stop and start. |
| `/etc/init.d/S99netmon status` | Check whether the application is running. Prints the PID if running, or "not running" otherwise. Exits with code 0 if running, 1 if not. |

### Process Details

- **Binary path:** `/usr/bin/lp_netmon`
- **Configuration argument:** `/etc/netmon.conf` (passed as the first argument)
- **PID file:** `/var/run/lp_netmon.pid`
- **Runs as:** `root` (required for raw ICMP sockets)
- **Runs in background:** Yes (via `start-stop-daemon -S -b`)

### Signal Handling

| Signal | Behavior |
|--------|----------|
| `SIGTERM` | Clean shutdown: saves history data to `/tmp/netmon_history.dat`, stops collectors, exits with code 0. |
| `SIGINT` | Same as `SIGTERM` (clean shutdown). |

---

## Disabling the Demo Application

The Luckfox Pico 86 ships with a demo application (`86UI_demo`) that uses the display. The `S99netmon` init script automatically kills this demo application when starting LP Network Monitor:

```sh
killall 86UI_demo 2>/dev/null || true
```

To permanently disable the demo application so it does not start on boot:

1. Identify its init script (commonly `/etc/init.d/S50launcher` or similar):

   ```bash
   ls /etc/init.d/S*
   ```

2. Disable it by removing the execute permission:

   ```bash
   chmod -x /etc/init.d/S50launcher
   ```

   Or rename it to prevent it from running:

   ```bash
   mv /etc/init.d/S50launcher /etc/init.d/_disabled_S50launcher
   ```

This ensures that only LP Network Monitor uses the display after boot.

---

## Configuration Reference

The configuration file is located at `/etc/netmon.conf` and uses a simple key-value format. One setting per line. Lines beginning with `#` or `;` are comments. Whitespace around the `=` sign is optional.

### All Configuration Options

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `ping_target` | IPv4 address | `10.0.0.1` | Target for ICMP echo (ping) probes. Should be your default gateway or a reliable host on your network. |
| `dns_target` | IPv4 address | `8.8.8.8` | DNS server to probe for connectivity testing. |
| `dns_test_hostname` | Hostname | `google.com` | Domain name used for DNS A-record resolution tests. |
| `alert_bw_warn_pct` | Integer (0-100) | `80` | Bandwidth utilization percentage that triggers a **warning** alert. |
| `alert_bw_crit_pct` | Integer (0-100) | `95` | Bandwidth utilization percentage that triggers a **critical** alert. |
| `alert_ping_warn_ms` | Integer (ms) | `100` | Ping latency in milliseconds that triggers a **warning** alert. |
| `alert_ping_crit_ms` | Integer (ms) | `500` | Ping latency in milliseconds that triggers a **critical** alert. |
| `alert_err_threshold` | Integer | `10` | Interface error rate (errors/second) that triggers a packet-error alert. |

### Configuration Limits

The configuration parser supports a maximum of 32 key-value entries. Keys can be up to 64 characters and values up to 128 characters. If the configuration file is missing or unreadable, all compiled-in defaults are used and a warning is logged.

### Example: Custom Configuration for a Home Network

```ini
# Home network configuration
ping_target = 192.168.1.1
dns_target = 1.1.1.1
dns_test_hostname = cloudflare.com

# Relaxed thresholds for home use
alert_bw_warn_pct = 90
alert_bw_crit_pct = 98
alert_ping_warn_ms = 200
alert_ping_crit_ms = 1000
alert_err_threshold = 50
```

### Applying Changes

Configuration is read once at startup. After editing `/etc/netmon.conf`, restart the service:

```bash
/etc/init.d/S99netmon restart
```

---

## Boot Integration

LP Network Monitor is designed to start automatically at boot via the Buildroot init.d system.

### How It Works

1. During boot, BusyBox init executes all scripts in `/etc/init.d/` that match the pattern `S##name` in numerical order.
2. `S99netmon` runs near the end of the boot sequence (priority 99), ensuring that networking is already initialized.
3. The script kills any running instance of the demo application, then starts `lp_netmon` as a background daemon using `start-stop-daemon`.

### Boot Sequence Timing

| Priority | Typical Script | Purpose |
|----------|---------------|---------|
| S01-S20 | System initialization | Mount filesystems, set hostname |
| S30-S50 | Network, demo app | Bring up interfaces, start demo |
| S99 | S99netmon | Start LP Network Monitor (kills demo first) |

### Verifying Boot Start

After a reboot, verify the application started:

```bash
/etc/init.d/S99netmon status
```

Or check the process list:

```bash
ps | grep lp_netmon
```

---

## Updating the Application

### Using deploy.sh

The simplest update method is to re-run the deploy script from your development host:

```bash
cd /path/to/LP_Network_Monitoring/deploy
./deploy.sh 10.3.38.19 luckfox
```

Then restart on the target:

```bash
ssh root@10.3.38.19 '/etc/init.d/S99netmon restart'
```

### Manual Update (Binary Only)

If you only need to update the binary and the configuration has not changed:

1. Build on the development host:

   ```bash
   cd build && make -j$(nproc)
   ```

2. Copy to the target:

   ```bash
   scp build/lp_netmon root@10.3.38.19:/usr/bin/lp_netmon
   ```

3. Restart:

   ```bash
   ssh root@10.3.38.19 '/etc/init.d/S99netmon restart'
   ```

### Preserving Configuration

The `deploy.sh` script overwrites `/etc/netmon.conf` on each deployment. If you have customized the configuration on the target, back it up before deploying:

```bash
ssh root@10.3.38.19 'cp /etc/netmon.conf /etc/netmon.conf.bak'
```

After deployment, restore your custom configuration:

```bash
ssh root@10.3.38.19 'cp /etc/netmon.conf.bak /etc/netmon.conf'
ssh root@10.3.38.19 '/etc/init.d/S99netmon restart'
```

---

## Troubleshooting

### Application Will Not Start

**Symptom:** `/etc/init.d/S99netmon start` reports OK but `status` says not running.

**Check the following:**

1. **Binary exists and is executable:**

   ```bash
   ls -la /usr/bin/lp_netmon
   file /usr/bin/lp_netmon
   ```

   Expected output should show an ARM ELF binary, approximately 700 KB.

2. **Run manually to see error output:**

   ```bash
   /usr/bin/lp_netmon /etc/netmon.conf
   ```

   This runs the application in the foreground so you can see any error messages directly.

3. **Framebuffer device exists:**

   ```bash
   ls -la /dev/fb0
   ```

   If `/dev/fb0` does not exist, the display driver is not loaded.

4. **Another application is using the framebuffer:**

   ```bash
   ps | grep -E '86UI_demo|lp_netmon'
   ```

   Kill any competing process: `killall 86UI_demo`

5. **Verify architecture compatibility:**

   ```bash
   file /usr/bin/lp_netmon
   ```

   Must show `ELF 32-bit LSB executable, ARM, EABI5`. If it shows a different architecture, the binary was not cross-compiled correctly.

### Display Issues

**Symptom:** Application starts but nothing appears on screen, or the display shows corruption.

1. **Check framebuffer configuration:**

   ```bash
   cat /sys/class/graphics/fb0/virtual_size
   cat /sys/class/graphics/fb0/bits_per_pixel
   ```

2. **Verify display resolution matches:** The application expects a 720x720 display. If the panel reports a different resolution, the application may not render correctly.

3. **Test the framebuffer directly:**

   ```bash
   cat /dev/urandom > /dev/fb0
   ```

   If the screen shows random pixels, the framebuffer is working.

### Touch Not Working

**Symptom:** Display shows the UI but tapping does nothing.

1. **Check that the touch device exists:**

   ```bash
   ls -la /dev/input/event0
   ```

2. **Test touch input events:**

   ```bash
   cat /dev/input/event0 | hexdump
   ```

   Tap the screen; you should see hex output when touching. Press Ctrl+C to stop.

3. **Check for a different event device:** The touch controller may be on a different event number:

   ```bash
   ls /dev/input/
   cat /proc/bus/input/devices
   ```

   Look for a device with `Name:` containing "Goodix" or "goodix-ts".

### No Network Data

**Symptom:** The dashboard shows interfaces but all values are 0 or "N/A".

1. **Verify interfaces are up:**

   ```bash
   ip link show
   ip addr show
   ```

2. **Check /proc/net/dev is readable:**

   ```bash
   cat /proc/net/dev
   ```

3. **Verify the application is running as root** (required for raw sockets and some /proc files):

   ```bash
   ps | grep lp_netmon
   ```

   The process should be owned by root.

4. **Check that traffic is flowing:**

   ```bash
   ping -c 3 8.8.8.8
   ```

   Then check the dashboard again; the ping traffic itself should register.

### Ping/DNS Probes Failing

**Symptom:** Gateway and DNS indicators show red even though the network is working.

1. **Verify the ping target is correct in the config:**

   ```bash
   cat /etc/netmon.conf | grep ping_target
   ```

   Ensure it matches your actual gateway.

2. **Test manually:**

   ```bash
   ping -c 3 10.0.0.1
   nslookup google.com 8.8.8.8
   ```

3. **Raw socket permissions:** ICMP ping requires root privileges. If the application is somehow running as a non-root user, pings will silently fail.

### Application Crashes or Freezes

1. **Check for core dump:**

   ```bash
   ls -la /tmp/core*
   ```

2. **Check system memory:**

   ```bash
   free
   cat /proc/meminfo
   ```

   If available memory is extremely low, the system may be OOM-killing the process.

3. **Check dmesg for kernel messages:**

   ```bash
   dmesg | tail -20
   ```

---

## Monitoring Resource Usage

LP Network Monitor is designed to be lightweight. Here are ways to monitor its resource consumption:

### Memory Usage

```bash
# RSS (Resident Set Size) - actual physical memory used
ps -o pid,vsz,rss,comm | grep lp_netmon
```

Expected RSS is approximately 3 MB (3,000 KB). If RSS grows significantly over time, this may indicate a problem.

### CPU Usage

```bash
# Snapshot CPU usage
top -b -n 1 | grep lp_netmon
```

The application should use minimal CPU (typically under 5%) during normal operation. CPU usage may spike briefly during collector runs.

### Disk Usage

The application writes history data to `/tmp/netmon_history.dat` on shutdown. This file's size depends on the number of interfaces but is typically small (under 100 KB).

```bash
ls -la /tmp/netmon_history.dat
du -h /usr/bin/lp_netmon
```

---

## Log Messages

LP Network Monitor logs to standard output/error. When running as a daemon (via the init script), log output goes to the system console or is discarded. To capture logs, run the application in the foreground.

### Log Levels

| Level | Description |
|-------|-------------|
| `DEBUG` | Verbose internal state information. Not shown by default. |
| `INFO` | Normal operational messages (startup, shutdown, initialization). |
| `WARN` | Non-fatal issues (missing config file, interface enumeration problems). |
| `ERROR` | Fatal or serious errors (display init failure, touch init failure). |

### Common Log Messages

| Message | Level | Meaning |
|---------|-------|---------|
| `LP Network Monitor starting...` | INFO | Application is initializing. |
| `Config file not found at /etc/netmon.conf, using defaults` | WARN | The configuration file was not found. All default values will be used. |
| `Display initialized: 720x720` | INFO | Framebuffer display driver initialized successfully. |
| `Touchscreen initialized` | INFO | Evdev touch input initialized successfully. |
| `Failed to initialize display` | ERROR | Could not open `/dev/fb0` or set up the framebuffer. The application will exit. |
| `Failed to initialize touchscreen` | ERROR | Could not open `/dev/input/event0`. The application will exit. |
| `Initialization complete, entering main loop` | INFO | All subsystems are ready and the main event loop is starting. |
| `Shutting down...` | INFO | SIGTERM or SIGINT received; beginning clean shutdown. |
| `Goodbye` | INFO | Clean shutdown complete. |

### Viewing Logs in Real Time

To see log output, stop the daemon and run the application in the foreground:

```bash
/etc/init.d/S99netmon stop
/usr/bin/lp_netmon /etc/netmon.conf
```

Press Ctrl+C to stop.

---

## Network Requirements

### Required Access

| Requirement | Reason |
|-------------|--------|
| **Root privileges** | Raw sockets for ICMP ping require `CAP_NET_RAW` or root. The application must run as root. |
| **`/proc` filesystem** | Network statistics are read from `/proc/net/dev`, `/proc/net/tcp`, `/proc/net/udp`, `/proc/net/arp`, and `/proc/net/wireless`. |
| **ioctl access** | Interface enumeration uses `SIOCGIFCONF`, `SIOCGIFFLAGS`, `SIOCGIFADDR`, `SIOCGIFNETMASK`, and `SIOCGIFHWADDR` ioctls. |
| **Network interfaces** | At least one non-loopback interface must be present for meaningful monitoring. |

### Firewall Considerations

If a firewall is configured on the device:

- **ICMP echo** (protocol 1) must be allowed outbound to the `ping_target`.
- **UDP port 53** must be allowed outbound to the `dns_target` for DNS probes.
- No inbound ports are required by the application itself.

### Supported Interface Types

The application monitors all non-loopback interfaces reported by the kernel:

- Wired Ethernet (`eth0`, `eth1`, etc.)
- WiFi (`wlan0`, etc.) -- with additional WiFi-specific statistics
- Virtual interfaces (`br0`, `veth*`, etc.) -- basic traffic statistics only
- USB network adapters -- if they appear as standard network interfaces

---

## Backup and Restore

### What to Back Up

| File | Location | Purpose |
|------|----------|---------|
| **Configuration** | `/etc/netmon.conf` | All user-customized settings. |
| **History data** | `/tmp/netmon_history.dat` | 24-hour traffic history (volatile; lost on reboot unless saved). |
| **Init script** | `/etc/init.d/S99netmon` | Service management script (only back up if customized). |

### Backup Procedure

```bash
# From the development host:
TARGET=root@10.3.38.19

# Back up configuration
scp ${TARGET}:/etc/netmon.conf ./backup_netmon.conf

# Back up history (optional, volatile data)
scp ${TARGET}:/tmp/netmon_history.dat ./backup_history.dat
```

### Restore Procedure

```bash
# Restore configuration
scp ./backup_netmon.conf ${TARGET}:/etc/netmon.conf

# Restore history (optional)
scp ./backup_history.dat ${TARGET}:/tmp/netmon_history.dat

# Restart to apply
ssh ${TARGET} '/etc/init.d/S99netmon restart'
```

### Factory Reset

To return to the default configuration, redeploy from the source repository:

```bash
cd /path/to/LP_Network_Monitoring/deploy
./deploy.sh 10.3.38.19 luckfox
ssh root@10.3.38.19 '/etc/init.d/S99netmon restart'
```

This replaces the binary, configuration, and init script with the versions from the repository.
