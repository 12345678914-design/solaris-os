ASM := nasm
CC  := gcc
LD  := ld

CFLAGS := -m64 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
          -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -Wall -Wextra -I. -MMD -MP

LDFLAGS := -m elf_x86_64 -T kernel/linker.ld -nostdlib

CFILES := kernel/kernel.c kernel/lib.c kernel/heap.c kernel/input.c \
          drivers/framebuffer.c drivers/vbe.c drivers/mouse.c drivers/rtc.c \
          fs/fsram.c \
          graphics/font.c graphics/logo.c \
          terminal/terminal.c \
          gui/gfx.c gui/wm.c gui/gui.c

OBJS := boot.o $(CFILES:.c=.o)
DEPS := $(CFILES:.c=.d)

all: os.iso

boot.o: boot/boot.asm
	$(ASM) -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

os.iso: kernel.bin boot/grub.cfg
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin
	cp boot/grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o os.iso iso

# En WSLg GTK intenta Wayland y falla; forzamos el backend X11.
run: os.iso
	GDK_BACKEND=x11 qemu-system-x86_64 -cdrom os.iso -vga std -m 512

# Alternativa sin GUI: conectar con un cliente VNC a localhost:5900
run-vnc: os.iso
	qemu-system-x86_64 -cdrom os.iso -vga std -m 512 -display none -vnc :0

clean:
	rm -f boot.o $(CFILES:.c=.o) $(DEPS) kernel.bin os.iso
	rm -rf iso/boot

.PHONY: all run run-vnc clean

-include $(DEPS)
