#include "common.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void bios_history_push(bios_history_t *h, float value) {
    h->values[h->head] = value;
    h->head = (h->head + 1) % BIOS_HISTORY_LEN;
    if (h->count < BIOS_HISTORY_LEN) {
        h->count++;
    }
}

float bios_history_get(const bios_history_t *h, int offset) {
    if (h->count == 0 || offset < 0 || offset >= h->count) {
        return 0.0f;
    }
    int idx = (h->head - 1 - offset + BIOS_HISTORY_LEN) % BIOS_HISTORY_LEN;
    return h->values[idx];
}

void bios_format_bytes(char *buf, size_t len, unsigned long long kb) {
    if (kb >= 1024ULL * 1024ULL) {
        snprintf(buf, len, "%.1f GB", kb / (1024.0 * 1024.0));
    } else if (kb >= 1024ULL) {
        snprintf(buf, len, "%.1f MB", kb / 1024.0);
    } else {
        snprintf(buf, len, "%llu KB", kb);
    }
}

void bios_format_uptime(char *buf, size_t len, long seconds) {
    long days = seconds / 86400;
    long hours = (seconds % 86400) / 3600;
    long mins = (seconds % 3600) / 60;
    if (days > 0) {
        snprintf(buf, len, "%ldd %02ldh %02ldm", days, hours, mins);
    } else {
        snprintf(buf, len, "%02ldh %02ldm", hours, mins);
    }
}

void bios_strncpy(char *dest, const char *src, size_t dest_size) {
    if (dest_size == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t n = 0;
    while (src[n] != '\0' && n + 1 < dest_size) {
        n++;
    }
    memcpy(dest, src, n);
    dest[n] = '\0';
}

void bios_trim(char *s) {
    if (!s) {
        return;
    }
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
}

int bios_read_file(const char *path, char *buf, size_t len) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    if (!fgets(buf, (int)len, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    bios_trim(buf);
    return 0;
}

long bios_read_long(const char *path) {
    char buf[64];
    if (bios_read_file(path, buf, sizeof(buf)) != 0) {
        return -1;
    }
    return strtol(buf, NULL, 10);
}

void bios_sleep_ms(int ms) {
    struct timespec req = {
        .tv_sec = (time_t)(ms / 1000),
        .tv_nsec = (long)(ms % 1000) * 1000000L,
    };
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
    }
}

size_t bios_snprintf_dots(char *buf, size_t buflen, const char *label,
                          const char *value, int total_width) {
    int label_len = (int)strlen(label);
    int value_len = (int)strlen(value);
    int dots = total_width - label_len - value_len - 1;
    if (dots < 1) {
        dots = 1;
    }
    char dots_buf[128];
    if (dots >= (int)sizeof(dots_buf)) {
        dots = (int)sizeof(dots_buf) - 1;
    }
    memset(dots_buf, '.', (size_t)dots);
    dots_buf[dots] = '\0';
    return (size_t)snprintf(buf, buflen, "%s %s %s", label, dots_buf, value);
}