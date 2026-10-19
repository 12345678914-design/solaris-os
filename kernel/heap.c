#include "kernel/heap.h"
#include "kernel/lib.h"

/* Heap estatico en BSS. GRUB reserva la region (p_memsz > p_filesz). */
#define HEAP_SIZE (20u * 1024u * 1024u)

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(16)));

typedef struct block {
    size_t        size;   /* bytes utiles del bloque */
    uint32_t      libre;
    uint32_t      _pad;
    struct block *next;
    struct block *prev;
} block_t;

#define HDR      ((size_t)sizeof(block_t))
#define ALIGN8(x) (((x) + 7u) & ~(size_t)7u)

static block_t *g_head;

void heap_init(void) {
    g_head = (block_t *)heap_area;
    g_head->size  = HEAP_SIZE - HDR;
    g_head->libre = 1;
    g_head->next  = 0;
    g_head->prev  = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;
    size = ALIGN8(size);

    for (block_t *b = g_head; b; b = b->next) {
        if (!b->libre || b->size < size) continue;

        /* Partir si sobra espacio para otro bloque + minimo razonable. */
        if (b->size >= size + HDR + 16) {
            block_t *nuevo = (block_t *)((uint8_t *)b + HDR + size);
            nuevo->size  = b->size - size - HDR;
            nuevo->libre = 1;
            nuevo->next  = b->next;
            nuevo->prev  = b;
            if (b->next) b->next->prev = nuevo;
            b->next = nuevo;
            b->size = size;
        }
        b->libre = 0;
        return (uint8_t *)b + HDR;
    }
    return 0;
}

void *kzalloc(size_t size) {
    void *p = kmalloc(size);
    if (p) memset(p, 0, size);
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)((uint8_t *)ptr - HDR);
    b->libre = 1;

    /* Coalescer con el siguiente. */
    if (b->next && b->next->libre) {
        block_t *n = b->next;
        b->size += HDR + n->size;
        b->next = n->next;
        if (n->next) n->next->prev = b;
    }
    /* Coalescer con el anterior. */
    if (b->prev && b->prev->libre) {
        block_t *p = b->prev;
        p->size += HDR + b->size;
        p->next = b->next;
        if (b->next) b->next->prev = p;
    }
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) { kfree(ptr); return 0; }

    block_t *b = (block_t *)((uint8_t *)ptr - HDR);
    if (b->size >= size) return ptr;   /* ya cabe */

    void *nuevo = kmalloc(size);
    if (!nuevo) return 0;
    memcpy(nuevo, ptr, b->size);
    kfree(ptr);
    return nuevo;
}

size_t heap_total(void) { return HEAP_SIZE; }

size_t heap_used(void) {
    size_t total = 0;
    for (block_t *b = g_head; b; b = b->next)
        if (!b->libre) total += b->size;
    return total;
}
