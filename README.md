# solaris-os

Sistema operativo de hobby para x86_64 (long mode), con kernel propio, terminal
de texto, sistema de archivos en RAM (`fsram`), lectura de **ext4** desde el
disco y un escritorio grafico con gestor de ventanas.

## Caracteristicas

- Arranque BIOS/UEFI via GRUB Multiboot2.
- Kernel en C + ensamblador NASM, modo largo de 64 bits.
- Framebuffer VBE/Bochs, doble buffer y MTRR write-combining.
- Terminal con fuente escalable.
- `fsram`: crear, leer, escribir, editar y borrar archivos en memoria.
- Driver ATA PIO (LBA28) y lectura de **ext4** (superbloque, grupos, inodos,
  extents y directorios) como disco principal, en modo solo lectura.
- Allocator de heap propio (`kmalloc`/`kfree` con coalescing).
- Entrada unificada de teclado y raton PS/2 (cursor limitado a la pantalla).
- Escritorio grafico: fondo degradado, barra de tareas, reloj RTC, menu de
  inicio, ventanas arrastrables con foco y boton de cerrar.
- Explorador grafico de ext4 con navegacion de carpetas y visor de archivos,
  explorador de `fsram` y ventana "Acerca de".

## Compilar y ejecutar

Requiere `nasm`, `gcc`, `ld`, `grub-mkrescue`, `xorriso` y `qemu-system-x86_64`.

```sh
make          # genera os.iso
make disk     # genera un disk.img ext4 de ejemplo (si no existe)
make run      # arranca en QEMU con disk.img como disco IDE
```

En la terminal escribe `help` para ver los comandos, `ext4` para listar la raiz
del disco y `gui` para abrir el escritorio. Dentro del escritorio, **ESC**
vuelve a la terminal.
