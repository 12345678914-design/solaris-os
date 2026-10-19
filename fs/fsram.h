#ifndef FSRAM_H
#define FSRAM_H

#include <stdint.h>
#include <stddef.h>

#define FS_MAX_FILES 128
#define FS_NAME_MAX  31

#define FS_OK           0
#define FS_ERR_NOSPACE  1
#define FS_ERR_NOTFOUND 2
#define FS_ERR_EXISTS   3
#define FS_ERR_NAME     4

typedef struct {
    char     name[FS_NAME_MAX + 1];
    uint8_t *data;
    uint32_t size;      /* bytes usados */
    uint32_t cap;       /* bytes reservados */
    int      used;
} fs_file_t;

void       fs_init(void);
int        fs_create(const char *name);
int        fs_delete(const char *name);
int        fs_write(const char *name, const void *data, uint32_t size);
int        fs_append(const char *name, const void *data, uint32_t size);
int        fs_exists(const char *name);
int        fs_index_of(const char *name);
fs_file_t *fs_get(int index);
int        fs_count(void);
uint32_t   fs_bytes_used(void);

#endif
