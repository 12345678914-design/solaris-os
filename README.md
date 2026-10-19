# solaris-os

Sistema operativo de hobby para x86_64 (long mode), con kernel propio, terminal
de texto, sistema de archivos en RAM (`fsram`) y un escritorio grafico con
gestor de ventanas.

## Caracteristicas

- Arranque BIOS/UEFI via GRUB Multiboot2.
- Kernel en C + ensamblador NASM, modo largo de 64 bits.
- Framebuffer VBE/Bochs, doble buffer y MTRR write-combining.
- Terminal con fuente escalable (25%).
- `fsram`: crear, leer, escribir, editar y borrar archivos en memoria.
- Allocator de heap propio (`kmalloc`/`kfree` con coalescing).
- Entrada unificada de teclado y raton PS/2.
- Escritorio grafico: fondo degradado, barra de tareas, reloj RTC, menu de
  inicio, ventanas arrastrables con foco y boton de cerrar.
- Explorador de archivos de `fsram` y ventana "Acerca de".

## Compilar y ejecutar

Requiere `nasm`, `gcc`, `ld`, `grub-mkrescue`, `xorriso` y `qemu-system-x86_64`.

```sh
make          # genera os.iso
make run      # arranca en QEMU
```

En la terminal escribe `help` para ver los comandos y `gui` para abrir el
escritorio. Dentro del escritorio, **ESC** vuelve a la terminal.
