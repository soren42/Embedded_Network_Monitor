#!/bin/bash
# deploy.sh - Build and deploy Embedded Network Monitor to target device
#
# Usage: ./deploy.sh [target_ip] [password]

set -e

TARGET_IP="${1:-10.3.38.19}"
TARGET_PASS="${2:-luckfox}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== Embedded Network Monitor Deploy ==="
echo "Target: root@${TARGET_IP}"
echo ""

# Build
echo "[1/4] Building..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/arm-rockchip.cmake"
make -j"$(nproc)"
echo "Binary size: $(du -h embedded_netmon | cut -f1)"
echo ""

# Deploy binary
echo "[2/4] Deploying binary..."
sshpass -p "${TARGET_PASS}" scp "${BUILD_DIR}/embedded_netmon" \
    "root@${TARGET_IP}:/usr/bin/embedded_netmon"

# Deploy config
echo "[3/4] Deploying config..."
sshpass -p "${TARGET_PASS}" scp "${PROJECT_DIR}/config/netmon.conf" \
    "root@${TARGET_IP}:/etc/netmon.conf"

# Deploy init script
echo "[4/4] Deploying init script..."
sshpass -p "${TARGET_PASS}" scp "${PROJECT_DIR}/deploy/S99netmon" \
    "root@${TARGET_IP}:/etc/init.d/S99netmon"
sshpass -p "${TARGET_PASS}" ssh "root@${TARGET_IP}" \
    "chmod +x /etc/init.d/S99netmon"

echo ""
echo "=== Deploy complete ==="
echo "To start: sshpass -p '${TARGET_PASS}' ssh root@${TARGET_IP} '/etc/init.d/S99netmon restart'"
