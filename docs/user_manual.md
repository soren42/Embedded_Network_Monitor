# LP Network Monitor -- User Manual

## Table of Contents

1. [Introduction](#introduction)
2. [Dashboard Screen](#dashboard-screen)
3. [Interface Detail Screen](#interface-detail-screen)
4. [Traffic Screen](#traffic-screen)
5. [Alerts Screen](#alerts-screen)
6. [Connections Screen](#connections-screen)
7. [Settings / More Screen](#settings--more-screen)
8. [Touch Navigation](#touch-navigation)
9. [Status LED Colors](#status-led-colors)
10. [Chart Colors](#chart-colors)
11. [Configuration File Reference](#configuration-file-reference)

---

## Introduction

LP Network Monitor (`lp_netmon`) is a real-time network monitoring application designed for the Luckfox Pico 86 Smart Panel. It provides a live, touch-driven dashboard that displays the health and performance of all network interfaces on the device, including wired Ethernet and WiFi connections.

The application runs on a 720x720 capacitive touchscreen and is organized into a series of screens accessible through a bottom navigation bar and tappable cards. It continuously collects network statistics from the Linux kernel, performs connectivity probes (ping and DNS), and raises alerts when conditions exceed configured thresholds.

### What LP Network Monitor Shows You

- **Interface status** -- which network interfaces are up, down, or experiencing issues.
- **Traffic rates** -- real-time download (RX) and upload (TX) speeds for every interface.
- **Traffic history** -- sparkline charts showing the last 60 seconds of activity, and full 24-hour historical charts at one-minute resolution.
- **Connectivity health** -- whether the default gateway is reachable and whether DNS resolution is working, along with latency measurements.
- **Active connections** -- a live table of all TCP and UDP connections on the device.
- **Alerts** -- automatic notifications when bandwidth is high, latency is excessive, interfaces go down, or errors accumulate.
- **WiFi details** -- SSID, signal strength, link quality, noise level, and wireless bitrate for WiFi interfaces.
- **ARP table** -- nearby devices discovered via ARP.

---

## Dashboard Screen

The Dashboard is the main screen you see when the application starts. It provides an at-a-glance overview of every network interface and the device's connectivity status.

### Interface Cards

Each detected network interface (e.g., `eth0`, `wlan0`) is displayed as a card. Each interface card contains:

| Element | Description |
|---------|-------------|
| **Interface name** | The Linux interface name (e.g., `eth0`, `wlan0`). |
| **Status LED** | A colored circle indicating interface health (see [Status LED Colors](#status-led-colors)). |
| **IP address** | The currently assigned IPv4 address, or "No IP" if none is assigned. |
| **RX rate** | Current download speed (e.g., "1.2 MB/s"). |
| **TX rate** | Current upload speed (e.g., "340 KB/s"). |
| **Sparkline chart** | A tiny line chart showing the last 60 seconds of traffic. Blue represents download (RX), green represents upload (TX). |

Tapping on any interface card navigates to the [Interface Detail Screen](#interface-detail-screen) for that interface.

### Connectivity Panel

Below the interface cards, the Dashboard displays a connectivity summary panel:

| Indicator | Description |
|-----------|-------------|
| **Gateway** | Shows whether the configured gateway (default: `10.0.0.1`) is reachable, with round-trip latency in milliseconds. |
| **DNS** | Shows whether the configured DNS server (default: `8.8.8.8`) is reachable and whether hostname resolution (default: `google.com`) is working, with latency. |

Each indicator has its own status LED showing green (reachable), yellow (high latency), or red (unreachable).

---

## Interface Detail Screen

The Interface Detail screen provides comprehensive information about a single network interface. You reach it by tapping on an interface card from the Dashboard.

### Network Information

| Field | Description |
|-------|-------------|
| **Name** | Linux interface name. |
| **Status** | Whether the interface is UP and RUNNING. |
| **IP Address** | The assigned IPv4 address. |
| **Netmask** | The subnet mask. |
| **Gateway** | The default gateway for this interface. |
| **MAC Address** | The hardware (Ethernet) address. |
| **Link Speed** | The negotiated link speed in Mbps (for wired interfaces). |

### Traffic Statistics

| Metric | Description |
|--------|-------------|
| **RX Rate** | Current download speed in bytes per second (auto-scaled to KB/s, MB/s, etc.). |
| **TX Rate** | Current upload speed. |
| **RX Packets/s** | Number of packets received per second. |
| **TX Packets/s** | Number of packets transmitted per second. |
| **RX Errors/s** | Rate of receive errors. Any non-zero value may indicate a hardware or driver problem. |
| **TX Errors/s** | Rate of transmit errors. |
| **Peak RX** | Highest download rate observed since the application started. |
| **Peak TX** | Highest upload rate observed since the application started. |
| **RX Today** | Total bytes received since midnight (or since the application started). |
| **TX Today** | Total bytes transmitted since midnight (or since the application started). |

### Cumulative Counters

| Counter | Description |
|---------|-------------|
| **Total RX Bytes** | Lifetime receive byte count from the kernel. |
| **Total TX Bytes** | Lifetime transmit byte count from the kernel. |
| **Total RX Packets** | Lifetime receive packet count. |
| **Total TX Packets** | Lifetime transmit packet count. |
| **RX Errors** | Cumulative receive errors. |
| **TX Errors** | Cumulative transmit errors. |
| **RX Dropped** | Packets dropped on receive (e.g., buffer full). |
| **TX Dropped** | Packets dropped on transmit. |
| **Collisions** | Ethernet collision count (relevant for half-duplex links). |

### 60-Second Traffic Chart

A line chart at the bottom of the detail screen shows the last 60 seconds of traffic at one-second resolution. The blue line is download (RX) and the green line is upload (TX). The Y-axis auto-scales to fit the current traffic levels.

### WiFi Information (Wireless Interfaces Only)

If the interface is a WiFi adapter, an additional section is displayed:

| Field | Description |
|-------|-------------|
| **SSID** | The name of the WiFi network currently associated. |
| **Signal** | Signal strength in dBm. Typical values: -30 dBm (excellent) to -80 dBm (poor). |
| **Link Quality** | A percentage representing overall link quality. |
| **Noise** | Background noise level in dBm. |
| **Bitrate** | The current wireless bitrate in Mbps. |

---

## Traffic Screen

The Traffic screen displays a large 24-hour traffic chart and daily traffic totals. Access it by tapping the **Traffic** tab in the bottom navigation bar.

### 24-Hour Chart

The chart spans the full width of the display and shows traffic history over the last 24 hours at one-minute resolution (1,440 data points). Two lines are plotted:

- **Blue line** -- Download (RX) rate.
- **Green line** -- Upload (TX) rate.

The X-axis represents time, going from 24 hours ago on the left to the present moment on the right. The Y-axis auto-scales to the peak rate observed in the 24-hour window.

### Interface Selector

If multiple network interfaces are present, a selector at the top of the screen allows you to choose which interface's traffic to display.

### Daily Totals

Below the chart, daily traffic totals are displayed:

- **RX Today** -- Total data downloaded today.
- **TX Today** -- Total data uploaded today.

---

## Alerts Screen

The Alerts screen shows a chronological log of network events and threshold violations. Access it by tapping the **Alerts** tab in the bottom navigation bar.

### Alert Types

| Alert Type | Description |
|------------|-------------|
| **Interface Down** | A previously active network interface has gone down. |
| **High Bandwidth** | Traffic on an interface has exceeded the configured bandwidth warning or critical threshold. |
| **Packet Errors** | The error rate on an interface has exceeded the configured threshold. |
| **Packet Drops** | Packet drops have exceeded the configured threshold. |
| **Gateway Unreachable** | The configured ping target (gateway) is not responding. |
| **DNS Failure** | The configured DNS server is not responding or cannot resolve the test hostname. |
| **High Latency** | Round-trip time to the gateway or DNS server exceeds the configured threshold. |

### Severity Levels

Each alert entry is tagged with a severity level, indicated by color:

| Severity | Color | Meaning |
|----------|-------|---------|
| **Info** | Blue/White | Informational event (e.g., interface came back up). |
| **Warning** | Yellow | A threshold has been crossed but the situation is not yet critical (e.g., latency above 100 ms). |
| **Critical** | Red | A serious condition requiring attention (e.g., gateway unreachable, latency above 500 ms). |

### Alert Hysteresis

The alert system uses hysteresis to prevent rapid toggling of alerts. A condition must persist for a configured number of consecutive checks (default: 3) before an alert is raised, and must clear for the same number of checks before the alert is dismissed. This prevents false alerts from brief transient conditions.

### Alert Badge

When alerts are active, a badge with the count of active alerts appears in the status bar at the top of every screen.

### Clearing Alerts

The Alerts screen provides a way to clear all active alerts. Once cleared, the alert badge resets to zero and the alert log retains the historical entries for reference.

The alert log stores up to 64 entries in a ring buffer. When full, the oldest entries are overwritten by new ones.

---

## Connections Screen

The Connections screen displays a live table of all active TCP and UDP connections on the device. It is accessible from the **More** screen.

### Table Columns

| Column | Description |
|--------|-------------|
| **Protocol** | `TCP` or `UDP`. |
| **Local Address** | The local IP address and port number (e.g., `10.3.38.19:22`). |
| **Remote Address** | The remote IP address and port number. For listening sockets, this is `0.0.0.0:0`. |
| **State** | The connection state (TCP only; UDP connections show as `ESTABLISHED` or blank). |

### TCP Connection States

| State | Description |
|-------|-------------|
| `ESTABLISHED` | Active, open connection with data flowing. |
| `LISTEN` | Waiting for incoming connections (server socket). |
| `TIME_WAIT` | Connection closed; waiting for delayed packets to expire. |
| `CLOSE_WAIT` | Remote side has closed; waiting for local application to close. |
| `SYN_SENT` | Connection attempt in progress (outbound). |
| `SYN_RECV` | Connection attempt received (inbound). |
| `FIN_WAIT1` | Local side has requested close, waiting for acknowledgment. |
| `FIN_WAIT2` | Local close acknowledged, waiting for remote close. |
| `CLOSING` | Both sides closing simultaneously. |
| `LAST_ACK` | Waiting for final acknowledgment of close. |

The connection data is read from `/proc/net/tcp` and `/proc/net/udp` and can display up to 256 connections.

---

## Settings / More Screen

The **More** screen (accessible via the rightmost tab in the bottom navigation bar) provides:

- **Connections** -- Tap to navigate to the [Connections Screen](#connections-screen).
- **Settings** -- View current configuration values loaded from `/etc/netmon.conf`.
- **System Information** -- System uptime, total memory, free memory, and available memory.

The Settings sub-screen displays the current values of all configuration options (ping target, DNS target, alert thresholds) in a read-only format. Configuration changes must be made by editing the configuration file directly and restarting the application.

---

## Touch Navigation

LP Network Monitor uses a straightforward touch-based navigation model on the 720x720 capacitive touchscreen.

### Bottom Navigation Bar

The bottom of the screen contains a persistent tab bar with four tabs:

| Tab | Screen | Description |
|-----|--------|-------------|
| **Dashboard** | Main overview | Interface cards and connectivity status. |
| **Traffic** | 24-hour charts | Historical traffic data. |
| **Alerts** | Alert log | Alert history and active alerts. |
| **More** | Additional screens | Connections, Settings, and system info. |

The currently selected tab is highlighted. Tap any tab to switch to that screen.

### Tapping Interface Cards

On the Dashboard, tapping an interface card opens the [Interface Detail Screen](#interface-detail-screen) for that interface with full statistics and a 60-second chart.

### Back Button

Sub-screens (Interface Detail, Connections, Settings) that are not directly part of the bottom tab bar display a back arrow button in the top-left corner. Tap the back button to return to the previous screen.

### Scrolling

Screens with content that exceeds the visible area (such as the Connections table or Alert log) support vertical scrolling. Swipe up or down to scroll through the content.

### Screen Layout

The screen is divided into three zones:

| Zone | Height | Description |
|------|--------|-------------|
| **Status Bar** | 50 px | Displays the alert badge and status indicators. |
| **Content Area** | 610 px | The main content for the current screen. |
| **Navigation Bar** | 60 px | The bottom tab bar for screen switching. |

---

## Status LED Colors

Circular LED indicators appear throughout the application to convey status at a glance:

| Color | Meaning | Examples |
|-------|---------|----------|
| **Green** | Up / OK / Healthy | Interface is up and running; gateway is reachable; DNS is working. |
| **Yellow** | Warning | Latency is elevated (above 100 ms but below 500 ms); bandwidth usage is high (above 80% but below 95%). |
| **Red** | Down / Error / Critical | Interface is down; gateway is unreachable; DNS failure; latency above 500 ms; bandwidth above 95%. |

---

## Chart Colors

All charts in the application use a consistent color scheme:

| Color | Hex Code | Meaning |
|-------|----------|---------|
| **Blue** (`#3498DB`) | RX / Download | Incoming traffic (data received by the interface). |
| **Green** (`#2ECC71`) | TX / Upload | Outgoing traffic (data transmitted by the interface). |

These colors are used in:
- Dashboard sparkline charts (60-second mini-charts on each interface card).
- Interface Detail 60-second chart.
- Traffic screen 24-hour chart.

---

## Configuration File Reference

LP Network Monitor reads its configuration from `/etc/netmon.conf` at startup. The file uses a simple INI-style format with one key-value pair per line. Lines starting with `#` or `;` are comments.

### Configuration Options

| Key | Default | Description |
|-----|---------|-------------|
| `ping_target` | `10.0.0.1` | IPv4 address to send ICMP echo requests to for gateway reachability checks. Set this to your network's default gateway. |
| `dns_target` | `8.8.8.8` | IPv4 address of the DNS server to probe for connectivity checks. |
| `dns_test_hostname` | `google.com` | Hostname used for DNS resolution tests. The application sends a DNS A-record query for this name. |
| `alert_bw_warn_pct` | `80` | Bandwidth usage percentage at which a warning-level alert is triggered. |
| `alert_bw_crit_pct` | `95` | Bandwidth usage percentage at which a critical-level alert is triggered. |
| `alert_ping_warn_ms` | `100` | Ping round-trip time in milliseconds above which a warning-level latency alert is triggered. |
| `alert_ping_crit_ms` | `500` | Ping round-trip time in milliseconds above which a critical-level latency alert is triggered. |
| `alert_err_threshold` | `10` | Number of interface errors per second that triggers a packet-error alert. |

### Example Configuration File

```ini
# LP Network Monitor Configuration
# Lines starting with # or ; are comments
# Format: key = value

# Ping target (gateway IP)
ping_target = 10.0.0.1

# DNS server for connectivity checks
dns_target = 8.8.8.8

# Hostname for DNS resolution test
dns_test_hostname = google.com

# Alert thresholds
alert_bw_warn_pct = 80
alert_bw_crit_pct = 95
alert_ping_warn_ms = 100
alert_ping_crit_ms = 500
alert_err_threshold = 10
```

### Applying Configuration Changes

Configuration is loaded once at application startup. To apply changes:

1. Edit `/etc/netmon.conf` on the target device.
2. Restart the application: `/etc/init.d/S99netmon restart`

If the configuration file is missing or cannot be read, the application uses compiled-in defaults for all values.
