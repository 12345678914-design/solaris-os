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
          drivers/ata.c \
          fs/fsram.c fs/ext4.c \
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

# Disco principal ext4 (opcional). Se crea con: make disk
DISK := disk.img
QEMU := qemu-system-x86_64 -cdrom os.iso -vga std -m 512
ifneq ($(wildcard $(DISK)),)
QEMU += -drive file=$(DISK),format=raw,if=ide,index=0
endif

# En WSLg GTK intenta Wayland y falla; forzamos el backend X11.
run: os.iso
	GDK_BACKEND=x11 $(QEMU)

# Alternativa sin GUI: conectar con un cliente VNC a localhost:5900
run-vnc: os.iso
	$(QEMU) -display none -vnc :0

# Crea una imagen ext4 de prueba a partir de disk_root/ (requiere e2fsprogs)
disk:
	rm -f $(DISK)
	dd if=/dev/zero of=$(DISK) bs=1M count=32 status=none
	mke2fs -q -t ext4 -d disk_root -L SOLARIS -O ^has_journal $(DISK)

clean:
	rm -f boot.o $(CFILES:.c=.o) $(DEPS) kernel.bin os.iso
	rm -rf iso/boot

.PHONY: all run run-vnc disk clean

-include $(DEPS)
