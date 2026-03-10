#!/bin/bash
# Dragoon QEMU launch script
QEMU=/opt/homebrew/bin/qemu-system-aarch64

exec $QEMU \
    -M virt \
    -cpu cortex-a53 \
    -m 128M \
    -device ramfb \
    -device virtio-keyboard-device \
    -device virtio-tablet-device \
    -serial stdio \
    -kernel dragoon.elf \
    "$@"
