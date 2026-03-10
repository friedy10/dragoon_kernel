# Dragoon OS

A capability-based ARM64 microkernel with virtual memory, symmetric multiprocessing, and a graphical desktop — built entirely from scratch.

Dragoon runs on QEMU's `virt` machine with up to 4 Cortex-A53 cores. It boots to a windowed desktop with a start menu, mouse cursor, and launchable applications including four games.

## Features

### Kernel

- **ARM64 microkernel** — runs at EL1 on QEMU virt (Cortex-A53)
- **Virtual memory** — per-task page tables (4-level, 4KB granule) with hardware ASID tagging; kernel identity-mapped across all address spaces
- **SMP** — boots up to 4 cores via PSCI; each core runs its own scheduler loop with per-CPU state (idle context, current task, reschedule flag)
- **Spinlocks** — ticket locks using ARMv8 exclusive instructions (LDAXR/STLXR) with IRQ-safe variants; protects all shared kernel state
- **Capability system** — typed capabilities (memory, IPC, IRQ, I/O, task) mediate resource access
- **Round-robin scheduler** — global run queue with spinlock; tasks are cooperatively or timer-preempted across all CPUs
- **IPC** — synchronous message passing with capability transfer
- **Wait queues** — generic blocking with timer-based timeout (10ms granularity)
- **Futex** — Linux-style atomic-word wait/wake built on wait queues
- **Sleep** — `sleep_ms()` / `sleep_ticks()` for timed blocking
- **IPI** — GIC SGI 0 wakes idle cores when tasks become runnable

### Desktop

- **Graphical desktop** — 640x480 framebuffer via ramfb, double-buffered
- **Window manager** — title bars, drag-to-move, click-to-focus, z-ordering
- **Start menu** — click "Start" to launch Terminal, Snake, Tetris, Raycaster, or Breakout
- **Virtio input** — keyboard (virtio-keyboard) and absolute-pointing tablet (virtio-tablet)

### Applications

- **Terminal** — kernel shell with command output
- **Snake** — classic snake game with score tracking
- **Tetris** — falling-block puzzle with piece preview
- **Raycaster** — Wolfenstein 3D-style first-person renderer
- **Breakout** — paddle-and-ball brick breaker

### Other

- **Linux compatibility layer** — partial POSIX syscall translation server
- **GICv2 interrupt controller** — distributor + per-CPU interface
- **ARM Generic Timer** — 10ms tick, per-CPU virtual timer (CNTV)

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Apps: Terminal, Snake, Tetris, Raycaster, ...    │
├──────────────────────────────────────────────────┤
│  Window Manager / GUI / Start Menu                │
├──────────────────────────────────────────────────┤
│  Linux Compat Server     │  Virtio Drivers        │
├──────────────────────────────────────────────────┤
│  SMP Scheduler │ IPC │ Capabilities │ VM / MMU    │
│  Spinlocks │ Wait Queues │ Futex │ Sleep          │
│  GICv2 IRQ │ Timer │ IPI (SGI)                    │
├──────────────────────────────────────────────────┤
│  ARM64 Hardware (QEMU virt, 4x Cortex-A53)        │
└──────────────────────────────────────────────────┘
```

### Memory Map

| Address Range | Description |
|---|---|
| `0x08000000` – `0x08010000` | GICv2 (distributor + CPU interface) |
| `0x09000000` | PL011 UART |
| `0x09020000` | fw_cfg (ramfb configuration) |
| `0x0A000000` + | Virtio MMIO devices (keyboard, tablet) |
| `0x40080000` | Kernel `.text` start |
| `0x40000000` – `0x48000000` | RAM (128 MB, identity-mapped) |

### SMP Boot Sequence

1. CPU 0 enters `_start`, drops from EL2 to EL1, zeroes BSS, calls `kernel_main()`
2. `kernel_main()` initializes all subsystems (MM, IRQ, timer, tasks, scheduler, etc.)
3. `smp_init(4)` calls PSCI `CPU_ON` (HVC) for CPUs 1–3
4. Each secondary enters `_secondary_start`: sets up its stack, MMU, GIC CPU interface, and timer
5. Each secondary calls `sched_start()` and begins pulling tasks from the global run queue
6. CPU 0 also enters `sched_start()` — all 4 cores now schedule tasks concurrently

### Virtual Memory Design

Each task gets its own L0 (PGD) page table with a unique 8-bit ASID. The kernel's identity mapping (RAM + MMIO) is shared by pointing every task's `pgd[0]` at the same L1 table. Context switches write the new task's `TTBR0_EL1` (page table base + ASID). The `mm_map_page()` function walks L0→L1→L2→L3, allocating intermediate tables on demand for 4KB page granularity.

## Prerequisites

- **QEMU** — `qemu-system-aarch64` (tested with 8.x+)
- **AArch64 bare-metal toolchain** — `aarch64-elf-gcc`

### macOS (Homebrew)

```bash
brew install qemu
brew install aarch64-elf-gcc
```

### Linux (Ubuntu/Debian)

```bash
sudo apt install qemu-system-arm gcc-aarch64-linux-gnu
```

On Linux, update `CROSS` in the Makefile:
```makefile
CROSS = aarch64-linux-gnu-
```

## Build

```bash
make clean && make
```

Produces `dragoon.elf` (~50 KB).

## Run

```bash
make run
```

Or manually:

```bash
qemu-system-aarch64 \
    -M virt -cpu cortex-a53 -smp 4 -m 128M \
    -device ramfb \
    -device virtio-keyboard-device \
    -device virtio-tablet-device \
    -serial stdio \
    -kernel dragoon.elf
```

You should see kernel boot messages on the serial console, including all 4 CPUs coming online, followed by the graphical desktop.

## Usage

| Action | How |
|---|---|
| Open an app | Click **Start** → select from menu |
| Move a window | Drag the title bar |
| Focus a window | Click on it |
| Close a window | Press **ESC** |
| Play games | Arrow keys (Snake/Tetris), WASD (Raycaster), A/D (Breakout) |

## Project Structure

```
kernel/
  boot.S            Boot code (EL2→EL1 drop, BSS zero, secondary CPU entry)
  vectors.S         Exception vectors, context switch, task trampoline
  main.c            Kernel entry, subsystem init
  mm.c / mm.h       Page allocator (bitmap), 4-level page tables, MMU, mm_map_page()
  vm.c / vm.h       Per-task address spaces, ASID allocation
  smp.c / smp.h     PSCI secondary boot, IPI via GIC SGI
  spinlock.h        Ticket spinlock (LDAXR/STLXR, IRQ-safe variants)
  percpu.h          Per-CPU data (current task, idle context, reschedule flag)
  task.c / task.h   Task create/destroy, per-task address space
  sched.c / sched.h SMP round-robin scheduler with global run queue
  cap.c / cap.h     Capability system (memory, IPC, IRQ, I/O, task)
  ipc.c / ipc.h     Synchronous message passing
  irq.c / irq.h     GICv2 setup, IRQ dispatch, IPI handler
  timer.c / timer.h ARM Generic Timer (10ms tick, per-CPU)
  waitqueue.c/h     Wait queues with timeout
  sleep.c / sleep.h Timed sleep
  futex.c / futex.h Futex (atomic word synchronization)
  syscall.c/h       SVC dispatch (yield, send, recv, cap, map, exit)
  gpu.c / gpu.h     Drawing primitives, render target redirection
  fb.c / fb.h       ramfb framebuffer init
  wm.c / wm.h       Window manager (title bars, z-order, compositing)
  gui.c / gui.h     Desktop, start menu, app lifecycle
  input.c / input.h Input event processing
  virtio.c / virtio.h       Virtio MMIO transport (legacy v1)
  virtio_input.c/h          Virtio keyboard + tablet drivers
  font.c / font.h   8x16 bitmap font
  uart.c / uart.h   PL011 serial output
  printf.c / printf.h       Kernel printf

linux/              Linux compatibility layer
  compat.c          POSIX syscall stubs
  server.c          Compat server task

games/              Windowed applications
  snake.c / snake.h
  tetris.c / tetris.h
  raycaster.c / raycaster.h
  breakout.c / breakout.h

drivers/hello/      Example driver
linker.ld           Linker script (kernel at 0x40080000)
Makefile            Cross-compilation build
run.sh              QEMU launch script
```

## License

MIT
