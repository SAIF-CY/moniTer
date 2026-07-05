#ifndef BIOS_MONITOR_PROCESS_H
#define BIOS_MONITOR_PROCESS_H

#include "common.h"

typedef struct {
    int pid;
    unsigned long long utime;
    unsigned long long stime;
} proc_sample_t;

void bios_process_init(void);
void bios_process_collect(bios_process_t *procs, int *count,
                          proc_sample_t *prev, int *prev_count,
                          unsigned long long total_jiffies,
                          unsigned long long total_mem_kb);

#endif /* BIOS_MONITOR_PROCESS_H */