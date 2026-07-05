#ifndef BIOS_MONITOR_DISK_H
#define BIOS_MONITOR_DISK_H

#include "common.h"

typedef struct {
    char name[BIOS_STR_SHORT];
    unsigned long long read_sectors;
    unsigned long long write_sectors;
    struct timespec ts;
} disk_sample_t;

void bios_disk_init(void);
void bios_disk_collect(bios_disk_info_t *disks, int *count,
                       disk_sample_t *prev, int *prev_count);

#endif /* BIOS_MONITOR_DISK_H */