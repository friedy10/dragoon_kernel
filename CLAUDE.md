# Dragoon Microkernel

Capability-based ARM64 microkernel with Linux compatibility layer.

## Build
```
make
```

## Run
```
./run.sh
```
Or: `make run`

## Debug
```
make debug
# Then: aarch64-elf-gdb dragoon.elf -ex "target remote :1234"
```

## Architecture

- Microkernel at EL1 with capability-based security
- All resources (memory, IPC, IRQs, MMIO) accessed through typed capabilities
- Synchronous IPC with capability transfer
- Linux compat layer translates Linux kernel API to capability operations
- Round-robin cooperative scheduler (10ms timer tick)
- ARM64 4KB granule page tables, identity-mapped kernel + MMIO

## Cross-compiler
`aarch64-elf-gcc` at `/opt/homebrew/bin/aarch64-elf-gcc`

## QEMU
`qemu-system-aarch64` at `/opt/homebrew/bin/qemu-system-aarch64`
Target: `-M virt -cpu cortex-a53 -m 128M`
