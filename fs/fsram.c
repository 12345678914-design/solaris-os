#include "fs/fsram.h"
#include "kernel/heap.h"
#include "kernel/lib.h"

static fs_file_t g_files[FS_MAX_FILES];

static int nombre_valido(const char *name) {
    if (!name || name[0] == '\0') return 0;
    size_t n = strlen(name);
    if (n > FS_NAME_MAX) return 0;
    for (size_t i = 0; i < n; i++) {
        if (name[i] == ' ' || name[i] == '\t' || name[i] == '\n' ||
            name[i] == '/' || name[i] == '\\')
            return 0;
    }
    return 1;
}

int fs_index_of(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].used && strcmp(g_files[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int slot_libre(void) {
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (!g_files[i].used) return i;
    return -1;
}

void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        g_files[i].used = 0;
        g_files[i].data = 0;
        g_files[i].size = 0;
        g_files[i].cap  = 0;
        g_files[i].name[0] = '\0';
    }
}

int fs_exists(const char *name) {
    return fs_index_of(name) >= 0;
}

int fs_create(const char *name) {
    if (!nombre_valido(name)) return FS_ERR_NAME;
    if (fs_exists(name))      return FS_ERR_EXISTS;
    int s = slot_libre();
    if (s < 0) return FS_ERR_NOSPACE;
    fs_file_t *f = &g_files[s];
    strncpy(f->name, name, FS_NAME_MAX);
    f->name[FS_NAME_MAX] = '\0';
    f->data = 0;
    f->size = 0;
    f->cap  = 0;
    f->used = 1;
    return FS_OK;
}

int fs_delete(const char *name) {
    int i = fs_index_of(name);
    if (i < 0) return FS_ERR_NOTFOUND;
    if (g_files[i].data) kfree(g_files[i].data);
    g_files[i].data = 0;
    g_files[i].size = 0;
    g_files[i].cap  = 0;
    g_files[i].used = 0;
    g_files[i].name[0] = '\0';
    return FS_OK;
}

/* Reserva capacidad de al menos 'necesario' bytes (crece a potencia de 2). */
static int reservar(fs_file_t *f, uint32_t necesario) {
    if (f->cap >= necesario) return FS_OK;
    uint32_t cap = 16;
    while (cap < necesario) cap <<= 1;
    uint8_t *nuevo = (uint8_t *)krealloc(f->data, cap);
    if (!nuevo) return FS_ERR_NOSPACE;
    f->data = nuevo;
    f->cap  = cap;
    return FS_OK;
}

int fs_write(const char *name, const void *data, uint32_t size) {
    int i = fs_index_of(name);
    if (i < 0) {
        int r = fs_create(name);
        if (r != FS_OK) return r;
        i = fs_index_of(name);
    }
    fs_file_t *f = &g_files[i];

    if (size == 0) { f->size = 0; return FS_OK; }

    int r = reservar(f, size);
    if (r != FS_OK) return r;
    memcpy(f->data, data, size);
    f->size = size;
    return FS_OK;
}

int fs_append(const char *name, const void *data, uint32_t size) {
    int i = fs_index_of(name);
    if (i < 0) {
        int r = fs_create(name);
        if (r != FS_OK) return r;
        i = fs_index_of(name);
    }
    fs_file_t *f = &g_files[i];
    if (size == 0) return FS_OK;

    int r = reservar(f, f->size + size);
    if (r != FS_OK) return r;
    memcpy(f->data + f->size, data, size);
    f->size += size;
    return FS_OK;
}

fs_file_t *fs_get(int index) {
    if (index < 0 || index >= FS_MAX_FILES) return 0;
    if (!g_files[index].used) return 0;
    return &g_files[index];
}

int fs_count(void) {
    int n = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (g_files[i].used) n++;
    return n;
}

uint32_t fs_bytes_used(void) {
    uint32_t total = 0;
    for (int i = 0; i < FS_MAX_FILES; i++)
        if (g_files[i].used) total += g_files[i].size;
    return total;
}
