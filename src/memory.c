#include "memory.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned long long total_kb;
    unsigned long long free_kb;
    unsigned long long avail_kb;
    unsigned long long buffers_kb;
    unsigned long long cached_kb;
    unsigned long long swap_total_kb;
    unsigned long long swap_free_kb;
} meminfo_raw_t;

static int parse_meminfo(meminfo_raw_t *raw) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        return -1;
    }

    memset(raw, 0, sizeof(*raw));
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long val = 0;
        if (sscanf(line, "MemTotal: %llu", &val) == 1) {
            raw->total_kb = val;
        } else if (sscanf(line, "MemFree: %llu", &val) == 1) {
            raw->free_kb = val;
        } else if (sscanf(line, "MemAvailable: %llu", &val) == 1) {
            raw->avail_kb = val;
        } else if (sscanf(line, "Buffers: %llu", &val) == 1) {
            raw->buffers_kb = val;
        } else if (sscanf(line, "Cached: %llu", &val) == 1) {
            raw->cached_kb = val;
        } else if (sscanf(line, "SwapTotal: %llu", &val) == 1) {
            raw->swap_total_kb = val;
        } else if (sscanf(line, "SwapFree: %llu", &val) == 1) {
            raw->swap_free_kb = val;
        }
    }
    fclose(f);
    return 0;
}

void bios_memory_init(bios_memory_info_t *mem) {
    memset(mem, 0, sizeof(*mem));
}

void bios_memory_collect(bios_memory_info_t *mem) {
    meminfo_raw_t raw;
    if (parse_meminfo(&raw) != 0) {
        return;
    }

    mem->total_kb = raw.total_kb;
    mem->free_kb = raw.free_kb;
    mem->avail_kb = raw.avail_kb;
    mem->swap_total_kb = raw.swap_total_kb;
    mem->swap_used_kb = raw.swap_total_kb - raw.swap_free_kb;

    if (mem->avail_kb > 0) {
        mem->used_kb = mem->total_kb - mem->avail_kb;
    } else {
        mem->used_kb = mem->total_kb - mem->free_kb - raw.buffers_kb - raw.cached_kb;
    }

    if (mem->total_kb > 0) {
        mem->usage_pct = 100.0 * (double)mem->used_kb / (double)mem->total_kb;
    }
    if (mem->swap_total_kb > 0) {
        mem->swap_pct = 100.0 * (double)mem->swap_used_kb / (double)mem->swap_total_kb;
    }

    bios_history_push(&mem->usage_history, (float)mem->usage_pct);
}