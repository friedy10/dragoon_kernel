# Dragoon OS

A capability-based ARM64 microkernel with a graphical desktop environment, built from scratch.

## Features

- **Microkernel architecture** — minimal kernel with capability-based security
- **ARM64 (AArch64)** — targets Cortex-A53 on QEMU virt machine
- **Memory management** — page allocator, page tables, MMU setup
- **Capability system** — memory and IPC capabilities with delegation
- **Cooperative scheduler** — round-robin with task states (ready, blocked, sleeping)
- **IPC** — synchronous message passing via endpoints
- **Wait queues** — generic blocking primitive with timeout support
- **Futex** — Linux-style atomic word wait/wake synchronization
- **Sleep** — timed task blocking (10ms granularity)
- **Graphical desktop** — ramfb framebuffer (640x480), window manager, start menu
- **Virtio input** — keyboard and tablet (absolute pointing) via virtio MMIO
- **Games** — Snake, Tetris, Raycaster (Wolfenstein-style), and Breakout
- **Linux compatibility layer** — partial POSIX syscall translation server

## Architecture

```
┌─────────────────────────────────────────────┐
│  Applications (Terminal, Snake, Tetris, ...) │
├─────────────────────────────────────────────┤
│  Window Manager / GUI                        │
├─────────────────────────────────────────────┤
│  Linux Compat Server    │  Drivers (virtio)  │
├─────────────────────────────────────────────┤
│  Kernel: Scheduler, IPC, Capabilities, MM    │
│  Wait Queues, Futex, Sleep, Timer, IRQ       │
├─────────────────────────────────────────────┤
│  ARM64 Hardware (QEMU virt, Cortex-A53)      │
└─────────────────────────────────────────────┘
```

## Prerequisites

- **QEMU** — `qemu-system-aarch64` (tested with 8.x+)
- **AArch64 cross-compiler** — `aarch64-elf-gcc` (bare-metal toolchain)

### macOS (Homebrew)

```bash
brew install qemu
brew install aarch64-elf-gcc
```

### Linux (Ubuntu/Debian)

```bash
sudo apt install qemu-system-arm gcc-aarch64-linux-gnu
```

> **Note:** On Linux, update `CROSS` in the Makefile to `aarch64-linux-gnu-` instead of `/opt/homebrew/bin/aarch64-elf-`.

## Building

```bash
make clean && make
```

This produces `dragoon.elf`.

## Running

```bash
make run
```

Or directly:

```bash
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -m 128M \
    -device ramfb \
    -device virtio-keyboard-device \
    -device virtio-tablet-device \
    -serial stdio \
    -kernel dragoon.elf
```

## Usage

The OS boots to a graphical desktop with a taskbar at the bottom.

- **Start menu** — click the "Start" button in the bottom-left corner to launch applications
- **Window management** — drag windows by their title bars, click to focus
- **Terminal** — basic kernel terminal with command output
- **Games** — Snake, Tetris, Raycaster, and Breakout are launchable from the start menu
- **ESC** — closes the currently focused window
- **Mouse** — virtio tablet provides absolute cursor positioning

## Project Structure

```
kernel/          Microkernel core
  boot.S         ARM64 boot code and exception vectors
  main.c         Kernel entry point and initialization
  mm.c           Memory management (page allocator, MMU)
  cap.c          Capability system
  task.c          Task management
  sched.c        Cooperative round-robin scheduler
  ipc.c          Inter-process communication
  irq.c          Interrupt handling
  timer.c        ARM Generic Timer (10ms tick)
  waitqueue.c    Wait queue with timeout support
  sleep.c        Sleep (timed blocking)
  futex.c        Futex (atomic word synchronization)
  gpu.c          Framebuffer and drawing primitives
  fb.c           ramfb device setup
  wm.c           Window manager
  gui.c          Desktop GUI, start menu, app dispatch
  input.c        Input event processing
  virtio.c       Virtio MMIO transport
  virtio_input.c Virtio keyboard/tablet drivers
  font.c         Built-in bitmap font
  uart.c         Serial console output
  printf.c       Kernel printf

linux/           Linux compatibility layer
  compat.c       POSIX syscall translation
  server.c       Compat server task

games/           Graphical applications
  snake.c        Snake game
  tetris.c       Tetris game
  raycaster.c    Wolfenstein-style raycaster
  breakout.c     Breakout game

drivers/         Userspace drivers
  hello/         Example driver
```

## License

MIT
