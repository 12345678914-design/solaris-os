#include "fs/ext4.h"
#include "drivers/ata.h"
#include "kernel/lib.h"

/* ------------------------------- Estado ---------------------------------- */
static uint32_t g_block_size;
static uint32_t g_inode_size;
static uint32_t g_inodes_per_group;
static uint32_t g_first_data_block;
static uint32_t g_desc_size;
static uint32_t g_incompat;
static int      g_mounted;

static uint8_t  g_node[4096];   /* bloque de nodo de extents */
static uint8_t  g_blk[4096];    /* bloque de datos        */

#define EXT4_EXTENTS_FL     0x00080000u
#define EXT4_EXT_MAGIC      0xF30Au
#define EXT4_FEATURE_INCOMPAT_64BIT 0x80u

/* --------------------------- Lectura little-endian ------------------------ */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* --------------------------- Lectura de bytes ----------------------------- */
static int read_bytes(uint64_t off, uint32_t len, void *out) {
    uint8_t sec[512];
    uint8_t *o = (uint8_t *)out;
    while (len) {
        uint32_t lba = (uint32_t)(off / 512);
        uint32_t so  = (uint32_t)(off % 512);
        uint32_t chunk = 512 - so;
        if (chunk > len) chunk = len;
        if (ata_read_sectors(lba, 1, sec) < 0) return -1;
        for (uint32_t i = 0; i < chunk; i++) o[i] = sec[so + i];
        o += chunk; off += chunk; len -= chunk;
    }
    return 0;
}

static uint32_t read_block(uint32_t block, uint8_t *buf) {
    if (read_bytes((uint64_t)block * g_block_size, g_block_size, buf) < 0)
        return 0;
    return 1;
}

/* ------------------------------- Montaje ---------------------------------- */
int ext4_init(void) {
    uint8_t sb[1024];
    if (!ata_present()) return -1;
    if (read_bytes(1024, 1024, sb) < 0) return -1;
    if (rd16(sb + 0x38) != 0xEF53u) return -1;

    uint32_t log_bs = rd32(sb + 0x18);
    g_block_size    = 1024u << log_bs;
    g_inode_size    = rd16(sb + 0x58);
    if (g_inode_size < 128) g_inode_size = 128;
    g_inodes_per_group = rd32(sb + 0x28);
    g_first_data_block = rd32(sb + 0x14);
    g_incompat         = rd32(sb + 0x60);

    g_desc_size = 32;
    if (g_incompat & EXT4_FEATURE_INCOMPAT_64BIT) g_desc_size = rd16(sb + 0xFE);
    if (g_desc_size < 32) g_desc_size = 32;

    if (g_block_size > 4096 || g_inode_size > 256) return -1;
    if (g_inodes_per_group == 0) return -1;

    g_mounted = 1;
    return 0;
}

int ext4_mounted(void) { return g_mounted; }
uint32_t ext4_root(void) { return 2; }

/* ------------------------------- Inodos ----------------------------------- */
static int read_inode(uint32_t ino, uint8_t *out) {
    if (ino == 0 || !g_mounted) return -1;
    uint32_t group = (ino - 1) / g_inodes_per_group;
    uint32_t idx   = (ino - 1) % g_inodes_per_group;

    uint64_t gd_off = ((uint64_t)g_first_data_block + 1) * g_block_size +
                      (uint64_t)group * g_desc_size;
    uint8_t gd[64];
    if (read_bytes(gd_off, g_desc_size, gd) < 0) return -1;

    uint32_t itable_lo = rd32(gd + 8);
    uint32_t itable_hi = (g_desc_size >= 64) ? rd32(gd + 0x28) : 0;
    uint64_t itable = ((uint64_t)itable_hi << 32) | itable_lo;

    uint64_t off = itable * g_block_size + (uint64_t)idx * g_inode_size;
    if (read_bytes(off, g_inode_size, out) < 0) return -1;
    return 0;
}

static uint64_t inode_size(const uint8_t *in) {
    uint32_t lo = rd32(in + 4);
    uint32_t hi = rd32(in + 0x6C);
    return ((uint64_t)hi << 32) | lo;
}

static uint32_t inode_flags(const uint8_t *in) { return rd32(in + 0x20); }
static uint16_t inode_mode(const uint8_t *in)  { return rd16(in + 0); }

/* ------------------------------- Extents ---------------------------------- */
static uint32_t extent_lookup(const uint8_t *node, uint32_t logical) {
    if (rd16(node) != EXT4_EXT_MAGIC) return 0;
    uint16_t entries = rd16(node + 2);
    uint16_t depth   = rd16(node + 6);
    const uint8_t *e = node + 12;

    if (depth == 0) {
        for (uint16_t i = 0; i < entries; i++) {
            const uint8_t *ee = e + (uint32_t)i * 12;
            uint32_t ee_block = rd32(ee);
            uint16_t ee_len   = rd16(ee + 4);
            uint32_t start = ((uint32_t)rd16(ee + 6) << 16) | rd32(ee + 8);
            uint32_t len = (ee_len > 32768) ? (ee_len - 32768) : ee_len;
            if (logical >= ee_block && logical < ee_block + len)
                return start + (logical - ee_block);
        }
        return 0;
    }

    for (uint16_t i = 0; i < entries; i++) {
        const uint8_t *ee = e + (uint32_t)i * 12;
        uint32_t ei_block = rd32(ee);
        uint32_t leaf = ((uint32_t)rd16(ee + 8) << 16) | rd32(ee + 4);
        uint32_t next = (i + 1 < entries) ? rd32(e + (uint32_t)(i + 1) * 12)
                                          : 0xFFFFFFFFu;
        if (logical >= ei_block && logical < next) {
            if (!read_block(leaf, g_node)) return 0;
            return extent_lookup(g_node, logical);
        }
    }
    return 0;
}

static uint32_t inode_map(const uint8_t *in, uint32_t logical) {
    if (inode_flags(in) & EXT4_EXTENTS_FL)
        return extent_lookup(in + 0x28, logical);
    return 0;   /* sin soporte de bloques indirectos clasicos */
}

/* ------------------------------- Directorios ------------------------------ */
int ext4_readdir(uint32_t dir_ino, int index, ext4_dirent_t *out) {
    uint8_t in[256];
    if (read_inode(dir_ino, in) < 0) return -1;
    if ((inode_mode(in) & 0xF000u) != 0x4000u) return -1;

    uint64_t size = inode_size(in);
    int count = 0;
    for (uint64_t off = 0; off < size; off += g_block_size) {
        uint32_t phys = inode_map(in, (uint32_t)(off / g_block_size));
        if (!phys) continue;
        if (!read_block(phys, g_blk)) return -1;

        uint32_t p = 0;
        while (p + 8 <= g_block_size) {
            uint32_t ino = rd32(g_blk + p);
            uint16_t rec = rd16(g_blk + p + 4);
            if (rec < 8) break;
            uint8_t nlen  = g_blk[p + 6];
            uint8_t ftype = g_blk[p + 7];
            if (ino != 0) {
                if (count == index) {
                    out->ino = ino;
                    out->type = ftype;
                    uint32_t n = nlen; if (n > 255) n = 255;
                    for (uint32_t i = 0; i < n; i++) out->name[i] = (char)g_blk[p + 8 + i];
                    out->name[n] = '\0';
                    return 1;
                }
                count++;
            }
            p += rec;
        }
    }
    return 0;
}

/* ------------------------------- Archivos --------------------------------- */
int ext4_read(uint32_t ino, uint8_t *buf, uint32_t max) {
    uint8_t in[256];
    if (read_inode(ino, in) < 0) return -1;
    uint64_t size = inode_size(in);
    uint32_t done = 0;

    for (uint64_t off = 0; off < size && done < max; off += g_block_size) {
        uint32_t want = g_block_size;
        if ((uint64_t)want > size - off) want = (uint32_t)(size - off);
        if (done + want > max) want = max - done;

        uint32_t phys = inode_map(in, (uint32_t)(off / g_block_size));
        if (!phys) {
            for (uint32_t i = 0; i < want; i++) buf[done + i] = 0;
        } else {
            if (!read_block(phys, g_blk)) return -1;
            for (uint32_t i = 0; i < want; i++) buf[done + i] = g_blk[i];
        }
        done += want;
    }
    return (int)done;
}

int ext4_isdir(uint32_t ino) {
    uint8_t in[256];
    if (read_inode(ino, in) < 0) return 0;
    return (inode_mode(in) & 0xF000u) == 0x4000u;
}

uint32_t ext4_size(uint32_t ino) {
    uint8_t in[256];
    if (read_inode(ino, in) < 0) return 0;
    return (uint32_t)inode_size(in);
}
