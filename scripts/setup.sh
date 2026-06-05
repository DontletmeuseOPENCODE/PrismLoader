#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "[PrismLoader] Setting up build environment..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[PrismLoader] Configuring with CMake..."
cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Release

echo "[PrismLoader] Building..."
cmake --build . -j"$(nproc)"

echo "[PrismLoader] Installing to system..."
sudo cmake --install .

echo "[PrismLoader] Done!"
echo ""
echo "  Run:  prism  (launches GD with PrismLoader)"
echo "  Logs: tail -f /tmp/prismloader.log"
