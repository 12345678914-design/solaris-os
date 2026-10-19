#include "kernel/lib.h"

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (n && ((uintptr_t)d & 7)) { *d++ = *s++; n--; }

    uint64_t *dw = (uint64_t *)d;
    const uint64_t *sw = (const uint64_t *)s;
    while (n >= 8) { *dw++ = *sw++; n -= 8; }

    d = (uint8_t *)dw;
    s = (const uint8_t *)sw;
    while (n) { *d++ = *s++; n--; }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t b = (uint8_t)c;
    uint64_t w = 0x0101010101010101ULL * b;

    while (n && ((uintptr_t)d & 7)) { *d++ = b; n--; }

    uint64_t *dw = (uint64_t *)d;
    while (n >= 8) { *dw++ = w; n -= 8; }

    d = (uint8_t *)dw;
    while (n) { *d++ = b; n--; }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) return dst;

    if (d < s) {
        /* Copia hacia adelante */
        while (n && ((uintptr_t)d & 7)) { *d++ = *s++; n--; }
        uint64_t *dw = (uint64_t *)d;
        const uint64_t *sw = (const uint64_t *)s;
        while (n >= 8) { *dw++ = *sw++; n -= 8; }
        d = (uint8_t *)dw;
        s = (const uint8_t *)sw;
        while (n) { *d++ = *s++; n--; }
    } else {
        /* Copia hacia atras */
        d += n;
        s += n;
        while (n && ((uintptr_t)d & 7)) { *--d = *--s; n--; }
        uint64_t *dw = (uint64_t *)d;
        const uint64_t *sw = (const uint64_t *)s;
        while (n >= 8) { *--dw = *--sw; n -= 8; }
        d = (uint8_t *)dw;
        s = (const uint8_t *)sw;
        while (n) { *--d = *--s; n--; }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst + strlen(dst);
    while ((*d++ = *src++)) { }
    return dst;
}

char *strchr(const char *s, int c) {
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (*s == '\0') return 0;
    }
}
