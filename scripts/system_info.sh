#!/bin/sh
set -eu

echo "=== Sistema ==="
uname -a

echo
echo "=== CPU ==="
if command -v lscpu >/dev/null 2>&1; then
    lscpu | grep -E '^(Architecture|CPU\(s\)|Core\(s\) per socket|Socket\(s\)|Model name|Flags):' || true
else
    grep -m1 'model name' /proc/cpuinfo || true
    grep -m1 '^flags' /proc/cpuinfo || true
fi

echo
echo "=== Toolchain ==="
clang-18 --version | head -n 1
opt-18 --version | head -n 1
mlir-opt-18 --version | head -n 1
mlir-translate-18 --version | head -n 1
