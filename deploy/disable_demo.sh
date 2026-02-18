#!/bin/sh
# disable_demo.sh - Disable the stock 86UI_demo app on the Luckfox Pico 86 Panel
#
# Usage: ./disable_demo.sh [target_ip] [password]

TARGET_IP="${1:-10.3.38.19}"
TARGET_PASS="${2:-luckfox}"

echo "Disabling 86UI_demo on ${TARGET_IP}..."

sshpass -p "${TARGET_PASS}" ssh "root@${TARGET_IP}" '
    # Stop the demo app
    killall 86UI_demo 2>/dev/null || true

    # Disable the init script by removing execute permission
    if [ -f /etc/init.d/S99lvgl ]; then
        chmod -x /etc/init.d/S99lvgl
        echo "Disabled /etc/init.d/S99lvgl"
    fi

    # Also check for other possible init script names
    for f in /etc/init.d/S*86UI* /etc/init.d/S*lvgl*; do
        if [ -f "$f" ] && [ -x "$f" ]; then
            chmod -x "$f"
            echo "Disabled $f"
        fi
    done

    echo "Demo app disabled."
'

echo "Done."
