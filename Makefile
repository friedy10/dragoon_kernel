# Dragoon Microkernel Makefile

CROSS   = /opt/homebrew/bin/aarch64-elf-
CC      = $(CROSS)gcc
AS      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
SIZE    = $(CROSS)size

# Flags
CFLAGS  = -Wall -Wextra -Wno-unused-parameter -ffreestanding -nostdlib \
          -nostartfiles -mgeneral-regs-only -mcpu=cortex-a53 -O2 -g
ASFLAGS = -ffreestanding -nostdlib -mcpu=cortex-a53
LDFLAGS = -nostdlib -T linker.ld

# Include paths
KERNEL_INC  = -I kernel
LINUX_INC   = -I linux/include
COMPAT_INC  = -I linux $(KERNEL_INC)
GAMES_INC   = -I kernel

# Kernel sources
KERNEL_C_SRC = kernel/main.c kernel/uart.c kernel/printf.c kernel/mm.c \
               kernel/cap.c kernel/task.c kernel/sched.c kernel/ipc.c \
               kernel/syscall.c kernel/irq.c kernel/timer.c \
               kernel/fb.c kernel/font.c kernel/gpu.c kernel/input.c \
               kernel/virtio.c kernel/virtio_input.c kernel/wm.c kernel/gui.c \
               kernel/waitqueue.c kernel/sleep.c kernel/futex.c \
               kernel/vm.c kernel/smp.c
KERNEL_S_SRC = kernel/boot.S kernel/vectors.S

# Linux compat sources
COMPAT_C_SRC = linux/compat.c linux/server.c

# Driver sources
DRIVER_C_SRC = drivers/hello/hello.c

# Game sources
GAMES_C_SRC = games/snake.c games/tetris.c games/raycaster.c games/breakout.c

# Object files
KERNEL_C_OBJ = $(KERNEL_C_SRC:.c=.o)
KERNEL_S_OBJ = $(KERNEL_S_SRC:.S=.o)
COMPAT_C_OBJ = $(COMPAT_C_SRC:.c=.o)
DRIVER_C_OBJ = $(DRIVER_C_SRC:.c=.o)
GAMES_C_OBJ  = $(GAMES_C_SRC:.c=.o)

ALL_OBJ = $(KERNEL_S_OBJ) $(KERNEL_C_OBJ) $(COMPAT_C_OBJ) $(DRIVER_C_OBJ) $(GAMES_C_OBJ)

# Output
TARGET = dragoon.elf
IMAGE  = dragoon.bin

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(ALL_OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJ)
	@echo "Built $(TARGET)"
	@$(SIZE) $@

$(IMAGE): $(TARGET)
	$(OBJCOPY) -O binary $< $@

# Kernel C files
kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) $(KERNEL_INC) -c $< -o $@

# Kernel assembly files
kernel/%.o: kernel/%.S
	$(AS) $(ASFLAGS) -c $< -o $@

# Linux compat files
linux/%.o: linux/%.c
	$(CC) $(CFLAGS) $(COMPAT_INC) $(LINUX_INC) -c $< -o $@

# Driver files
drivers/hello/%.o: drivers/hello/%.c
	$(CC) $(CFLAGS) $(LINUX_INC) -c $< -o $@

# Game files
games/%.o: games/%.c
	$(CC) $(CFLAGS) $(GAMES_INC) -c $< -o $@

clean:
	rm -f $(ALL_OBJ) $(TARGET) $(IMAGE)

run: $(TARGET)
	./run.sh

debug: $(TARGET)
	./run.sh -s -S
