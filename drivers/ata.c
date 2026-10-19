#include "drivers/ata.h"
#include "kernel/io.h"

#define ATA_DATA   0x1F0
#define ATA_ERROR  0x1F1
#define ATA_SECCNT 0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DRIVE  0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD    0x1F7

#define ST_BSY 0x80
#define ST_DRQ 0x08
#define ST_ERR 0x01

static void ata_delay400(void) {
    for (int i = 0; i < 4; i++) inb(ATA_STATUS);
}

static int ata_wait_ready(void) {
    for (uint32_t t = 0; t < 1000000; t++) {
        uint8_t st = inb(ATA_STATUS);
        if (st == 0) return -1;           /* sin disco */
        if (!(st & ST_BSY)) return 0;
    }
    return -1;
}

int ata_present(void) {
    outb(ATA_DRIVE, 0xA0);                /* primario maestro */
    ata_delay400();
    outb(ATA_SECCNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_CMD, 0xEC);                  /* IDENTIFY */
    uint8_t st = inb(ATA_STATUS);
    if (st == 0) return 0;
    /* Esperar hasta que baje BSY. */
    for (uint32_t t = 0; t < 1000000; t++) {
        st = inb(ATA_STATUS);
        if (!(st & ST_BSY)) break;
    }
    /* Si DRQ o ERR no aparecen -> no hay dispositivo ATA. */
    if (inb(ATA_LBA1) != 0 || inb(ATA_LBA2) != 0) return 0;
    for (uint32_t t = 0; t < 1000000; t++) {
        st = inb(ATA_STATUS);
        if (st & ST_ERR) return 0;
        if (st & ST_DRQ) {
            for (int i = 0; i < 256; i++) inw(ATA_DATA);
            return 1;
        }
    }
    return 0;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buf) {
    if (count == 0) return -1;
    if (ata_wait_ready() < 0) return -1;

    outb(ATA_DRIVE, 0xE0 | (uint8_t)((lba >> 24) & 0x0F));
    outb(ATA_SECCNT, count);
    outb(ATA_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD, 0x20);                  /* READ SECTORS */

    uint16_t *p = (uint16_t *)buf;
    for (uint8_t s = 0; s < count; s++) {
        if (ata_wait_ready() < 0) return -1;
        if (inb(ATA_STATUS) & ST_ERR) return -1;
        /* Esperar DRQ */
        int ok = 0;
        for (uint32_t t = 0; t < 1000000; t++) {
            uint8_t st = inb(ATA_STATUS);
            if (st & ST_ERR) return -1;
            if (st & ST_DRQ) { ok = 1; break; }
        }
        if (!ok) return -1;
        for (int i = 0; i < 256; i++) p[s * 256 + i] = inw(ATA_DATA);
    }
    return 0;
}
