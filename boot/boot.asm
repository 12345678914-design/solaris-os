; ============================================================================
; Solaris 3.0 - Arranque Multiboot2 + transicion a long mode (x86_64)
; GRUB nos entrega el control en modo protegido 32-bit:
;   eax = magic Multiboot2 (0x36D76289)
;   ebx = direccion fisica de la estructura Multiboot2
; Nosotros configuramos paging, PAE y GDT64 y saltamos a 64-bit.
; ============================================================================

bits 32

; ------------------------- Cabecera Multiboot2 ------------------------------
section .multiboot
align 8
mb2_start:
    dd 0xE85250D6                                   ; magic
    dd 0                                            ; arquitectura: i386 (protegido)
    dd mb2_end - mb2_start                          ; longitud de cabecera
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))    ; checksum

    ; --- Etiqueta de modo framebuffer (tipo 5) ---
    align 8
    dw 5                                            ; type = framebuffer
    dw 1                                            ; flags: 1 = opcional
    dd 20                                           ; size
    dd 1366                                         ; width
    dd 768                                          ; height
    dd 32                                           ; depth (bits por pixel)

    ; --- Etiqueta de fin (tipo 0) ---
    align 8
    dw 0
    dw 0
    dd 8
mb2_end:

; ------------------------- BSS: paginas + pila ------------------------------
section .bss
align 4096
pml4:          resb 4096
pdpt:          resb 4096
pdts:          resb 4096 * 4
align 16
stack_bottom:  resb 65536
stack_top:

; ------------------------- Datos temporales --------------------------------
section .data
align 4
magic_save:    dd 0
info_save:     dd 0

; ------------------------- GDT para long mode -------------------------------
section .rodata
align 8
gdt64:
    dq 0x0000000000000000              ; 0x00 descriptor nulo
    dq 0x00AF9A000000FFFF              ; 0x08 codigo 64-bit
    dq 0x00CF92000000FFFF              ; 0x10 datos
gdt64_end:
gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

; ------------------------- Codigo -------------------------------------------
section .text
global _start
extern kernel_main

_start:
    cli
    cld
    mov [magic_save], eax              ; guardar magic
    mov [info_save], ebx               ; guardar dir. multiboot2

    ; ---- Limpiar PML4 + PDPT + 4 PDTs (24 KB) ----
    mov ecx, 6 * 1024                  ; 6 paginas * 1024 dwords
    mov edi, pml4
    xor eax, eax
    rep stosd

    ; ---- PML4[0] -> PDPT ----
    mov eax, pdpt
    or  eax, 0x3                       ; present | writable
    mov [pml4], eax

    ; ---- PDPT[0..3] -> PDTS ----
    xor ecx, ecx
.pdpt_fill:
    mov eax, ecx
    shl eax, 12                        ; i * 4096
    add eax, pdts
    or  eax, 0x3
    mov [pdpt + ecx*8], eax
    inc ecx
    cmp ecx, 4
    jb  .pdpt_fill

    ; ---- Rellenar 4 PDTs con paginas de 2 MB (mapea 0..4 GB identico) ----
    xor ecx, ecx                       ; indice de PDT
.pdt_fill:
    mov eax, ecx
    shl eax, 12
    add eax, pdts                      ; base del PDT actual
    xor ebx, ebx                       ; indice de entrada
.pde_fill:
    mov edx, ecx
    shl edx, 9                         ; * 512
    add edx, ebx
    shl edx, 21                        ; * 2 MB -> direccion fisica
    or  edx, 0x83                      ; present | writable | PS (huge)
    mov [eax + ebx*8], edx
    inc ebx
    cmp ebx, 512
    jb  .pde_fill
    inc ecx
    cmp ecx, 4
    jb  .pdt_fill

    ; ---- Habilitar PAE ----
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; ---- Cargar CR3 ----
    mov eax, pml4
    mov cr3, eax

    ; ---- Habilitar long mode (EFER.LME) ----
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    ; ---- Habilitar paging (entra en long mode compatibility) ----
    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    ; ---- Cargar GDT y saltar a codigo 64-bit ----
    lgdt [gdt64_ptr]
    mov eax, [magic_save]
    mov edi, eax
    mov esi, [info_save]
    jmp 0x08:long_mode_start

; ------------------------- Codigo 64-bit ------------------------------------
bits 64
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
